typedef struct {
    MIB_TCPROW connection_row;  //Connection info
    bool is_valid;
} GW2ConnectionCache;

DWORD gw2_ping; 
GW2ConnectionCache gw2_conn = {0};
PMIB_TCPTABLE_OWNER_PID tcp_table = NULL;  //TCP Buffer
DWORD tcp_table_capacity = 0; //Buffer Size

void print_ip_from_decimal(DWORD decimal_ip)
{
    unsigned char *bytes = (unsigned char*)&decimal_ip;
    
    char buf[64];
    sprintf_s(buf, sizeof(buf), "IP: %d.%d.%d.%d\n", 
             bytes[0], bytes[1], bytes[2], bytes[3]);
    DEBUG_LOG(buf);
}

static DWORD last_rescan_tick = 0;
static bool no_admin = false;
#define RESCAN_INTERVAL_MS 5000

bool enable_tcp_estats(MIB_TCPROW *row)
{
    TCP_ESTATS_PATH_RW_v0 rw = {0};
    rw.EnableCollection = TRUE;
    DWORD result = SetPerTcpConnectionEStats(
        row,
        TcpConnectionEstatsPath,
        (PUCHAR)&rw, 0, sizeof(rw),
        0
    );

    if (result == ERROR_ACCESS_DENIED) {
    DEBUG_LOG("No admin privileges, cannot measure ping\n");
    no_admin = true;
    return false;
    }

    return result == NO_ERROR;
}

bool find_gw2_connection(allocator_i *allocator)
{
    DWORD table_size = tcp_table_capacity;
    
    DWORD result = GetExtendedTcpTable(tcp_table, &table_size, FALSE, AF_INET,
                                       TCP_TABLE_OWNER_PID_ALL, 0);
    
    if (result == ERROR_INSUFFICIENT_BUFFER || tcp_table == NULL) {
        tcp_table = (PMIB_TCPTABLE_OWNER_PID)allocator->realloc(
            allocator->inst,
            tcp_table,
            table_size,
            tcp_table_capacity
        );
        
        if (tcp_table == NULL) {
            tcp_table_capacity = 0;
            return false;
        }
        
        tcp_table_capacity = table_size;
        
        result = GetExtendedTcpTable(tcp_table, &table_size, FALSE, AF_INET,
                                     TCP_TABLE_OWNER_PID_ALL, 0);
    }
    
    if (result != NO_ERROR) {
        return false;
    }
    //Find process ID
    DWORD gw2_pid = 0;
    HWND gw2_window = FindWindowA(NULL, "Guild Wars 2");
    if (gw2_window != NULL) {
        GetWindowThreadProcessId(gw2_window, &gw2_pid);
        char buf[128];
        sprintf_s(buf, sizeof(buf), "Guild Wars 2 PID: %d\n", gw2_pid);
        //DEBUG_LOG(buf);
    }
    
    if (gw2_pid == 0) {
        gw2_conn.is_valid = false;
        DEBUG_LOG("Guild Wars 2 not found or not open\n");
        return false;
    }

    DWORD best_rtt = MAXDWORD;
    bool found = false;

    for (DWORD i = 0; i < tcp_table->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID *row = &tcp_table->table[i];
        
        if (row->dwOwningPid != gw2_pid) continue;
        if (row->dwState != MIB_TCP_STATE_ESTAB) continue;
        if (row->dwRemotePort != 57367) continue;

        MIB_TCPROW probe = {0};
        probe.dwState      = row->dwState;
        probe.dwLocalAddr  = row->dwLocalAddr;
        probe.dwLocalPort  = row->dwLocalPort;
        probe.dwRemoteAddr = row->dwRemoteAddr;
        probe.dwRemotePort = row->dwRemotePort;

        if (!enable_tcp_estats(&probe)) continue;

        TCP_ESTATS_PATH_ROD_v0 path_rod = {0};
        DWORD estat_result = GetPerTcpConnectionEStats(
            &probe,
            TcpConnectionEstatsPath,
            NULL, 0, 0,
            NULL, 0, 0,
            (PUCHAR)&path_rod, 0, sizeof(path_rod)
        );

        if (estat_result != NO_ERROR) continue;
        if (path_rod.SampleRtt == 0 || path_rod.SampleRtt == MAXDWORD) continue;

        char buf[128];
        sprintf_s(buf, sizeof(buf), "Candidate connection RTT: %u\n", path_rod.SampleRtt);
        DEBUG_LOG(buf);

        if (path_rod.SampleRtt < best_rtt) {
            best_rtt = path_rod.SampleRtt;
            gw2_conn.connection_row = probe;
            found = true;
        }
    }

    gw2_conn.is_valid = found;
    return found;
}

bool find_gw2_ping(DWORD *out_rtt_ms)
{
    if (!gw2_conn.is_valid) {
        return false;
    }
    
    TCP_ESTATS_PATH_ROD_v0 path_rod = {0};
    DWORD result = GetPerTcpConnectionEStats(
        &gw2_conn.connection_row,
        TcpConnectionEstatsPath,
        NULL, 0, 0,
        NULL, 0, 0,
        (PUCHAR)&path_rod, 0, sizeof(path_rod)
    );
    
    if (result != NO_ERROR) {
        gw2_conn.is_valid = false;
        DEBUG_LOG("GW2 connection lost, will rescan\n");
        return false;
    }

    if (path_rod.SampleRtt == 0 || path_rod.SampleRtt == MAXDWORD) {
        gw2_conn.is_valid = false;
        DEBUG_LOG("GW2 connection RTT went stale, will rescan\n");
        return false;
    }

    char buf[128];
    sprintf_s(buf, sizeof(buf), "GW2 Ping: %u\n", path_rod.SampleRtt);
    DEBUG_LOG(buf);

    *out_rtt_ms = path_rod.SampleRtt;
    return true;
}

bool get_gw2_ping(allocator_i *allocator, DWORD *out_rtt_ms)
{
    if (no_admin) return false;

    DWORD now = GetTickCount();
    bool force_rescan = (now - last_rescan_tick) > RESCAN_INTERVAL_MS;

    if (force_rescan || !gw2_conn.is_valid) {
        last_rescan_tick = now;
        if (!find_gw2_connection(allocator)) return false;
    }

    return find_gw2_ping(out_rtt_ms);
}