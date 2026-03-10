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
    char buf[128];
    //Find process ID
    DWORD gw2_pid = 0;
    HWND gw2_window = FindWindowA(NULL, "Guild Wars 2");
    if (gw2_window != NULL) {
        GetWindowThreadProcessId(gw2_window, &gw2_pid);
		sprintf_s(buf, sizeof(buf), "Guild wars 2 pID is : %d\n", gw2_pid);
        DEBUG_LOG(buf);
    }
    
    if (gw2_pid == 0) {
        gw2_conn.is_valid = false;
		DEBUG_LOG("Guild wars 2 not found or is not open \n");
        return false;
    }
    
    //Find connection
    for (DWORD i = 0; i < tcp_table->dwNumEntries; i++) {
        MIB_TCPROW_OWNER_PID *row = &tcp_table->table[i];
        
        if (row->dwOwningPid == gw2_pid && row->dwState == MIB_TCP_STATE_ESTAB && row->dwRemotePort == 57367) //Remote port 6112 decimal little endian
        {
            gw2_conn.connection_row.dwState = row->dwState;
            gw2_conn.connection_row.dwLocalAddr = row->dwLocalAddr;
            gw2_conn.connection_row.dwLocalPort = row->dwLocalPort;
            gw2_conn.connection_row.dwRemoteAddr = row->dwRemoteAddr;
            gw2_conn.connection_row.dwRemotePort = row->dwRemotePort;
            
            gw2_conn.is_valid = true;
            return true;
        }
    }
    
    gw2_conn.is_valid = false;
    return false;
}

bool find_gw2_ping(DWORD *out_rtt_ms)
{
    if (!gw2_conn.is_valid) {
        return false;
    }
    
    //Stats from connection
    TCP_ESTATS_PATH_ROD_v0 path_rod = {0};
    ULONG rod_size = sizeof(path_rod);
    DWORD result = GetPerTcpConnectionEStats(
        &gw2_conn.connection_row,
        TcpConnectionEstatsPath,
        NULL, 0, 0,
        NULL, 0, 0,
        (PUCHAR)&path_rod,
        0,
        rod_size
    );
    
        char buf[128];

    if (result == NO_ERROR) {
        *out_rtt_ms = path_rod.SampleRtt; //Ping
		print_ip_from_decimal(gw2_conn.connection_row.dwRemoteAddr);
        char buf[128];
		sprintf_s(buf, sizeof(buf), "GW2 Ping is: %d\n", *out_rtt_ms);
        DEBUG_LOG(buf);
        return true;
    } else {
        gw2_conn.is_valid = false;
		DEBUG_LOG("Couldn't find ping, GW2 connection not active");
        return false;
    }
}

bool get_gw2_ping(allocator_i *allocator, DWORD *out_rtt_ms)
{
    if (find_gw2_ping(out_rtt_ms)) {
        return true;
    }
    
    if (find_gw2_connection(allocator)) {
        return find_gw2_ping(out_rtt_ms);
    }
    
    return false;
}