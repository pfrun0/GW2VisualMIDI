typedef enum {
    MODE_NORMAL,
    MODE_REBINDING_OCTAVE_DOWN,  
    MODE_REBINDING_OCTAVE_UP,  
    MODE_REBINDING_NOTES 
} AppMode;

typedef struct {
    WORD scancode;
    bool use_shift;
    bool use_ctrl;
    bool use_alt;
} KeyCombo;

AppMode app_mode = MODE_NORMAL;
int rebind_target_note = -1;
KeyCombo octave_up_combo = {11, false, false, false};    //Default '0'
KeyCombo octave_down_combo = {10, false, false, false};  //Default '9'

KeyCombo key_map[13] = {
    {2, false, false, false}, // '1' - C
    {2, true, false, false}, // 'Shift+1' - C#
    {3, false, false, false}, // '2' - D
    {3, true, false, false}, // 'Shift+2' - D#
    {4, false, false, false}, // '3' - E
    {5, false, false, false}, // '4' - F
    {4, true, false, false}, // 'Shift+3' - F#
    {6, false, false, false}, // '5' - G
    {5, true, false, false}, // 'Shift+4' - G#
    {7, false, false, false}, // '6' - A
    {6, true, false, false}, // 'Shift+5' - A#
    {8, false, false, false}, // '7' - B
    {9, false, false, false}  // '8' - next C
};

char display_map[13] = {
    '1', '1', '2', '2', '3', '4', '3', '5', '4', '6', '5', '7', '8'
};

typedef struct {
    float tex_x, tex_y;   //coords in the original texture (top-left)
    float tex_w, tex_h;   //size in the original texture
    int is_black;
    int is_pressed;
	int midi_note;
} PianoKey;


PianoKey piano_keys[NUM_KEYS];

static KeyCombo note_combo_map[128]; 

int key_pressed_note[128];

int active_octave = 3;

int cstarter_note()
{
	int offset = 3-active_octave;
	int starter_note =(active_octave*13)+21+offset;
	return starter_note;
}
void get_combo_display_string(KeyCombo *combo, char display_char, char *out_buf, size_t buf_size);

void press_key_combo(KeyCombo *combo)
{
    INPUT inputs[4] = {0};
    int input_count = 0;
    
    //Press modifiers first
    if (combo->use_ctrl) {
        inputs[input_count].type = INPUT_KEYBOARD;
        inputs[input_count].ki.wScan = 29;  //Left Ctrl scancode
        inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE;
        input_count++;
    }
    
    if (combo->use_alt) {
        inputs[input_count].type = INPUT_KEYBOARD;
        inputs[input_count].ki.wScan = 56;  //Left Alt scancode
        inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE;
        input_count++;
    }
    
    if (combo->use_shift) {
        inputs[input_count].type = INPUT_KEYBOARD;
        inputs[input_count].ki.wScan = 42;  //Left Shift scancode
        inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE;
        input_count++;
    }
    
    //Press main key
    inputs[input_count].type = INPUT_KEYBOARD;
    inputs[input_count].ki.wScan = combo->scancode;
    inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE;
    input_count++;
    
    SendInput(input_count, inputs, sizeof(INPUT));

}

void release_key_combo(KeyCombo *combo)
{
    INPUT inputs[4] = {0};
    int input_count = 0;
    
    //Release main key
    inputs[input_count].type = INPUT_KEYBOARD;
    inputs[input_count].ki.wScan = combo->scancode;
    inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
    input_count++;
    
    //Release modifiers
    if (combo->use_shift) {
        inputs[input_count].type = INPUT_KEYBOARD;
        inputs[input_count].ki.wScan = 42;
        inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        input_count++;
    }
    
    if (combo->use_alt) {
        inputs[input_count].type = INPUT_KEYBOARD;
        inputs[input_count].ki.wScan = 56;
        inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        input_count++;
    }
    
    if (combo->use_ctrl) {
        inputs[input_count].type = INPUT_KEYBOARD;
        inputs[input_count].ki.wScan = 29;
        inputs[input_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
        input_count++;
    }
    
    SendInput(input_count, inputs, sizeof(INPUT));

}

void shift_octave(bool direction)
{
    if(direction)
    {
        active_octave+=1;

        if(active_octave<=4 && active_octave>=2)
        {
            press_key_combo(&octave_up_combo);
			release_key_combo(&octave_up_combo);

            char display_char = (char)MapVirtualKeyA(MapVirtualKeyA(octave_up_combo.scancode, MAPVK_VSC_TO_VK), MAPVK_VK_TO_CHAR);
            if (display_char < 32 || display_char >= 127) display_char = '?';
            
            char combo_str[128];
            get_combo_display_string(&octave_up_combo, display_char, combo_str, sizeof(combo_str));
            
            if (midi_display_length < MAX_INPUT_LENGTH - 20) {
                char buf[32];
                sprintf_s(buf, sizeof(buf), "%s ", combo_str);
                strcat_s(midi_key_display, MAX_INPUT_LENGTH, buf);
                midi_display_length = (int)strlen(midi_key_display);
                
                if (midi_display_length > viewport_start + viewport_max_chars) {
                    viewport_start = midi_display_length - viewport_max_chars;
                }
                
                text_needs_update = 1;
            }
        }
    }
    else
    {
        active_octave-=1;
        
        INPUT in[2] = {0};

        if(active_octave>=2 && active_octave<=3)
        {
			press_key_combo(&octave_down_combo);
			release_key_combo(&octave_down_combo);

			char display_char = (char)MapVirtualKeyA(MapVirtualKeyA(octave_down_combo.scancode, MAPVK_VSC_TO_VK), MAPVK_VK_TO_CHAR);
            if (display_char < 32 || display_char >= 127) display_char = '?';
            
            char combo_str[128];
            get_combo_display_string(&octave_down_combo, display_char, combo_str, sizeof(combo_str));
            
            if (midi_display_length < MAX_INPUT_LENGTH - 20) {
                char buf[32];
                sprintf_s(buf, sizeof(buf), "%s ", combo_str);
                strcat_s(midi_key_display, MAX_INPUT_LENGTH, buf);
                midi_display_length = (int)strlen(midi_key_display);
                
                if (midi_display_length > viewport_start + viewport_max_chars) {
                    viewport_start = midi_display_length - viewport_max_chars;
                }
                
                text_needs_update = 1;
            }
        }
    }
}

void save_keybindings(const char *filename) {
    FILE *f;
    fopen_s(&f, filename, "w");
    if (!f) {
        OutputDebugStringA("Failed to save keybindings\n");
        return;
    }
    fprintf(f, "OCTAVE_DELAY\n");
    fprintf(f, "%d\n", octave_shift_delay);

    fprintf(f, "OCTAVE_KEYS\n");
    fprintf(f, "%d,%d,%d,%d,%d,%d,%d,%d\n", 
            octave_down_combo.scancode,
            octave_down_combo.use_shift ? 1 : 0,
            octave_down_combo.use_ctrl ? 1 : 0,
            octave_down_combo.use_alt ? 1 : 0,
            octave_up_combo.scancode,
            octave_up_combo.use_shift ? 1 : 0,
            octave_up_combo.use_ctrl ? 1 : 0,
            octave_up_combo.use_alt ? 1 : 0);
	
	fprintf(f, "NOTE_KEYS\n");

    for (int i = 0; i < 13; i++) {
        fprintf(f, "%d,%c,%d,%d,%d\n", 
                key_map[i].scancode, 
                display_map[i],
                key_map[i].use_shift ? 1 : 0,
                key_map[i].use_ctrl ? 1 : 0,
                key_map[i].use_alt ? 1 : 0);
    }
    
    fclose(f);
    OutputDebugStringA("Keybindings saved!\n");
}

void load_keybindings(const char *filename) {
    FILE *f;
    fopen_s(&f, filename, "r");
    if (!f) {
        OutputDebugStringA("No saved keybindings found, using defaults\n");
        return;
    }
    
    char line[128];
    int mode = 0;
    int note_idx = 0;
    
    while (fgets(line, sizeof(line), f)) {
        if (strstr(line, "OCTAVE_DELAY")) {
            mode = 1;
        } else if (strstr(line, "OCTAVE_KEYS")) {
            mode = 2;
        } else if (strstr(line, "NOTE_KEYS")) {
            mode = 3;
            note_idx = 0;
        } else if (mode == 1) {
            int delay;
            if (sscanf_s(line, "%d", &delay) == 1) {
                if (delay >= 20 && delay <= 500) {
                    octave_shift_delay = delay;
                    
                    char buf[128];
                    sprintf_s(buf, sizeof(buf), "Loaded octave delay: %dms\n", octave_shift_delay);
                    OutputDebugStringA(buf);
                } else {
                    OutputDebugStringA("Invalid octave delay in file, using default\n");
                }
            }
            mode = 0;
        } else if (mode == 2) {
            int down_sc, down_shift, down_ctrl, down_alt;
            int up_sc, up_shift, up_ctrl, up_alt;
            
            if (sscanf_s(line, "%d,%d,%d,%d,%d,%d,%d,%d", 
                        &down_sc, &down_shift, &down_ctrl, &down_alt,
                        &up_sc, &up_shift, &up_ctrl, &up_alt) == 8) {
                octave_down_combo.scancode = (WORD)down_sc;
                octave_down_combo.use_shift = down_shift != 0;
                octave_down_combo.use_ctrl = down_ctrl != 0;
                octave_down_combo.use_alt = down_alt != 0;
                
                octave_up_combo.scancode = (WORD)up_sc;
                octave_up_combo.use_shift = up_shift != 0;
                octave_up_combo.use_ctrl = up_ctrl != 0;
                octave_up_combo.use_alt = up_alt != 0;
            }
            mode = 0;
        } else if (mode == 3) {
            int scancode, use_shift, use_ctrl, use_alt;
            char display_char;
            if (sscanf_s(line, "%d,%c,%d,%d,%d", 
                        &scancode, &display_char, 1, 
                        &use_shift, &use_ctrl, &use_alt) == 5) {
                if (note_idx < 13) {
                    key_map[note_idx].scancode = (WORD)scancode;
                    key_map[note_idx].use_shift = use_shift != 0;
                    key_map[note_idx].use_ctrl = use_ctrl != 0;
                    key_map[note_idx].use_alt = use_alt != 0;
                    display_map[note_idx] = display_char;
                    note_idx++;
                }
            }
        }
    }
    
    fclose(f);
    OutputDebugStringA("Keybindings loaded!\n");
}

void restore_default_keybindings() {
    octave_shift_delay = 70;

    octave_down_combo.scancode = 10;
    octave_down_combo.use_shift = false;
    octave_down_combo.use_ctrl = false;
    octave_down_combo.use_alt = false;
    
    octave_up_combo.scancode = 11;
    octave_up_combo.use_shift = false;
    octave_up_combo.use_ctrl = false;
    octave_up_combo.use_alt = false;
    
    KeyCombo defaults[13] = {
        {2, false, false, false}, {2, true, false, false},
        {3, false, false, false}, {3, true, false, false},
        {4, false, false, false}, {5, false, false, false},
        {4, true, false, false},  {6, false, false, false},
        {5, true, false, false},  {7, false, false, false},
        {6, true, false, false},  {8, false, false, false},
        {9, false, false, false}
    };
    
    char default_display[13] = {
        '1', '1', '2', '2', '3', '4', '3', '5', '4', '6', '5', '7', '8'
    };
    
    for (int i = 0; i < 13; i++) {
        key_map[i] = defaults[i];
        display_map[i] = default_display[i];
    }
    
    OutputDebugStringA("Keybindings restored to defaults!\n");
}

KeyCombo* midi_note_to_key_combo(int note, int starter_note)
{
    int offset = note - starter_note;
    if (offset < 0 || offset > 12)
        return NULL;
    
    return &key_map[offset];
}

char midi_note_to_display_char(int note, int starter_note)
{
    int offset = note - starter_note;
    if (offset < 0 || offset > 12)
        return '?';
    
    return display_map[offset];
}

void get_combo_display_string(KeyCombo *combo, char display_char, char *out_buf, size_t buf_size)
{
    char temp[128] = "";
    
    if (combo->use_ctrl) {
        strcat_s(temp, sizeof(temp), "Ctrl+");
    }
    if (combo->use_alt) {
        strcat_s(temp, sizeof(temp), "Alt+");
    }
    if (combo->use_shift) {
        strcat_s(temp, sizeof(temp), "Shift+");
    }
    
    char ch_str[2] = {display_char, '\0'};
    strcat_s(temp, sizeof(temp), ch_str);
    
    strcpy_s(out_buf, buf_size, temp);
}