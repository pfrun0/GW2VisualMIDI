#pragma comment(linker, "/subsystem:windows")

#include <iostream>
#include <ws2tcpip.h>
#include <Windows.h>
#include <iphlpapi.h>
#pragma comment(lib, "iphlpapi.lib")

#include <assert.h>
#include <KHR/khrplatform.h>
#include <GLAD/glad.h> 
#include <mmsystem.h>
#pragma comment(lib, "winmm.lib")

#include "allocator.h"
#include "shader_compile.h"

#include "texture.h"
#include "piano_keys.h"

#include "fonts.h"

#include "cJSON.h"
#include <stdlib.h>

#define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
#define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
#define WGL_CONTEXT_PROFILE_MASK_ARB  0x9126
#define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x00000001
#define WGL_CONTEXT_COMPATIBILITY_PROFILE_BIT_ARB 0x00000002


#define NUM_WHITE_KEYS 51
#define NUM_BLACK_KEYS 36
#define NUM_KEYS (NUM_WHITE_KEYS + NUM_BLACK_KEYS)

#define MAX_INPUT_LENGTH 500000
char *input_text = NULL;
char *midi_key_display = NULL;
int input_length = 0;
int midi_display_length = 0; 
int text_needs_update = 1;
int viewport_start = 0; 
int viewport_max_chars = 70;

bool editing_octave_delay = false;
char octave_delay_input[16] = "";
int octave_delay_input_length = 0;
int octave_shift_delay = 70; //Default octave switch delay

#include "gw2_ping.inl"  
#include "key_press.inl"


void clear_display()
{
    strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "");
				midi_display_length = 0;
				viewport_start = 0;
};

typedef HGLRC (WINAPI * PFNWGLCREATECONTEXTATTRIBSARBPROC)(HDC, HGLRC, const int*);
PFNWGLCREATECONTEXTATTRIBSARBPROC wglCreateContextAttribsARB = NULL;

LRESULT CALLBACK Wndproc(
	HWND window_handle,
	UINT message,
	WPARAM w_param,
	LPARAM l_param)
{
	LRESULT Result = 0;
	switch (message)
	{
		case WM_CREATE:
		{
			HDC device_context = GetDC(window_handle);

			PIXELFORMATDESCRIPTOR pix_form_desc;			
			memset(&pix_form_desc, 0, sizeof PIXELFORMATDESCRIPTOR);

			pix_form_desc.nSize = sizeof(PIXELFORMATDESCRIPTOR);
			pix_form_desc.nVersion = 1;
			pix_form_desc.iPixelType = PFD_TYPE_RGBA;
			pix_form_desc.dwFlags = (PFD_SUPPORT_OPENGL | PFD_DOUBLEBUFFER | PFD_DRAW_TO_WINDOW);
			pix_form_desc.cDepthBits = 24;
			pix_form_desc.cStencilBits = 8;

			int pixel_format = ChoosePixelFormat( device_context, &pix_form_desc);
			BOOL pix_form_set = SetPixelFormat(device_context,pixel_format ,&pix_form_desc);
			
			HGLRC temp_context = wglCreateContext(device_context);
			BOOL MakeCurrContext = wglMakeCurrent(device_context, temp_context);		
			wglCreateContextAttribsARB = (PFNWGLCREATECONTEXTATTRIBSARBPROC)wglGetProcAddress("wglCreateContextAttribsARB");

			int op_gl_3_attribs[] = {
				WGL_CONTEXT_MAJOR_VERSION_ARB, 3,
				WGL_CONTEXT_MINOR_VERSION_ARB, 3,
				WGL_CONTEXT_PROFILE_MASK_ARB, WGL_CONTEXT_CORE_PROFILE_BIT_ARB, 
				0 
			};

			HGLRC op_gl_3_context = wglCreateContextAttribsARB(device_context, 0, op_gl_3_attribs);
			wglMakeCurrent(NULL, NULL);
			wglDeleteContext(temp_context);
			wglMakeCurrent(device_context, op_gl_3_context);
			
			gladLoadGL();
			
		}break;

		case WM_EXITSIZEMOVE:
		{
			HMONITOR mon = MonitorFromWindow(window_handle, MONITOR_DEFAULTTONEAREST);

			MONITORINFO mi = {0};
			mi.cbSize = sizeof(mi);
			GetMonitorInfo(mon, &mi);

			int monitor_w = mi.rcMonitor.right - mi.rcMonitor.left;
			int monitor_h = mi.rcMonitor.bottom - mi.rcMonitor.top;

			int client_w = monitor_w;
			int client_h = (int)(1903.0f / 6613.0f * client_w);

			DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;

			RECT r = {0,0,client_w,client_h};
			AdjustWindowRect(&r, style, FALSE);

			int win_w = r.right - r.left;
			int win_h = r.bottom - r.top;

			SetWindowPos(
				window_handle,
				NULL,
				mi.rcMonitor.left,
				mi.rcMonitor.top,
				win_w,
				win_h,
				SWP_NOZORDER
			);
		}break;

		case WM_SIZE:
		{
			int width  = LOWORD(l_param);
			int height = HIWORD(l_param);

			glViewport(0, 0, width, height);

			OutputDebugStringA("WM_SIZE\n");
		}break;
		
		case WM_CHAR: {
			if (editing_octave_delay) {
				if (w_param >= '0' && w_param <= '9') {
					if (octave_delay_input_length < 15) {
						octave_delay_input[octave_delay_input_length++] = (char)w_param;
						octave_delay_input[octave_delay_input_length] = '\0';
						
						char buf[128];
						sprintf_s(buf, sizeof(buf), "Enter octave delay (ms): %s", octave_delay_input);
						strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
						text_needs_update = 1;
					}
				}
				return 0;
			}

			if (w_param >= 32 && w_param < 127) {
			if (input_length < MAX_INPUT_LENGTH - 1) {
				input_text[input_length++] = (char)w_param;
				input_text[input_length] = '\0';
				
				//Auto-scroll to keep current visible
				if (input_length > viewport_start + viewport_max_chars) {
					viewport_start = input_length - viewport_max_chars;
				}
				
				text_needs_update = 1;
			}
			}
		} break;

		case WM_SYSKEYDOWN: {
			//F10 to set octave switch delay
			if (w_param == VK_F10 && rebind_target_note == -1) {
				editing_octave_delay = true;
				octave_delay_input_length = 0;
				octave_delay_input[0] = '\0';
				
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "Enter octave delay (ms) and press Enter to confirm: ");
				text_needs_update = 1;
				
				OutputDebugStringA("Entering octave delay edit mode\n");
				return 0;
			}

		} break;

		case WM_KEYDOWN: {
			bool ctrl_pressed = (GetKeyState(VK_CONTROL) & 0x8000) != 0;
			bool shift_pressed = (GetKeyState(VK_SHIFT) & 0x8000) != 0;
			bool alt_pressed = (GetKeyState(VK_MENU) & 0x8000) != 0;
			
			if (editing_octave_delay) {
				if (w_param == VK_RETURN) {
					if (octave_delay_input_length > 0) {
						int new_delay = atoi(octave_delay_input);
						
						//Validate range
						if (new_delay >= 20 && new_delay <= 500) {
							octave_shift_delay = new_delay;
							
							char buf[128];
							sprintf_s(buf, sizeof(buf), "Octave delay set to: %dms Save with F6", octave_shift_delay);
							strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
							
							OutputDebugStringA(buf);
						} else {
							strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "Invalid delay! Must be 20-500ms");
						}
					}
					
					editing_octave_delay = false;
					octave_delay_input_length = 0;
					octave_delay_input[0] = '\0';
					text_needs_update = 1;
					return 0;
				}
				
				if (w_param == VK_ESCAPE) {
					editing_octave_delay = false;
					octave_delay_input_length = 0;
					octave_delay_input[0] = '\0';
					strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "Octave delay edit cancelled");
					text_needs_update = 1;
					return 0;
				}
				
				if (w_param == VK_BACK) {
					if (octave_delay_input_length > 0) {
						octave_delay_input_length--;
						octave_delay_input[octave_delay_input_length] = '\0';
						
						char buf[128];
						sprintf_s(buf, sizeof(buf), "Enter octave delay (ms): %s", octave_delay_input);
						strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
						text_needs_update = 1;
					}
					return 0;
				}
				
				return 0;
			}

			//F6 to save keybindings
			if (w_param == VK_F6 && rebind_target_note == -1) {
                clear_display();
				save_keybindings("keybindings.txt");
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "Keybindings saved!");
				text_needs_update = 1;
				return 0;
			}

			//F7 to clear text display
			if (w_param == VK_F7 && rebind_target_note == -1) {
				clear_display();
				text_needs_update = 1;
				OutputDebugStringA("Text display cleared!\n");
				return 0;
			}

			//F8 to toggle rebinding mode
    		if (w_param == VK_F8 && app_mode == MODE_NORMAL) {
                clear_display();
				app_mode = MODE_REBINDING_OCTAVE_DOWN;
				OutputDebugStringA("REBINDING MODE: Set octave DOWN key\n");
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, 
						"REBIND MODE - Press key for OCTAVE DOWN");
				text_needs_update = 1;
				return 0;
			}

			//F8 to exit rebinding mode (from any rebind state)
			if (w_param == VK_F8 && app_mode != MODE_NORMAL) {
				app_mode = MODE_NORMAL;
				rebind_target_note = -1;
				OutputDebugStringA("REBINDING MODE DISABLED\n");
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "Rebinding finished save with F6!");
				text_needs_update = 1;
				return 0;
			}

			if (app_mode == MODE_REBINDING_OCTAVE_DOWN) {
				if (w_param == VK_SHIFT || w_param == VK_CONTROL || 
					w_param == VK_MENU || w_param == VK_LSHIFT || 
					w_param == VK_RSHIFT || w_param == VK_LCONTROL || 
					w_param == VK_RCONTROL || w_param == VK_LMENU ||
					w_param == VK_RMENU) {
					return 0;
				}
				
				if (w_param >= VK_F6 && w_param <= VK_F11) {
					return 0;
				}
				
				WORD scancode = (WORD)((l_param >> 16) & 0x1FF);
				octave_down_combo.scancode = scancode;
				octave_down_combo.use_shift = shift_pressed;
				octave_down_combo.use_ctrl = ctrl_pressed;
				octave_down_combo.use_alt = alt_pressed;
				
				char ch = (char)MapVirtualKeyA(w_param, MAPVK_VK_TO_CHAR);
				if (ch < 32 || ch >= 127) ch = '?';
				
				char combo_str[128];
   				get_combo_display_string(&octave_down_combo, ch, combo_str, sizeof(combo_str));

				char buf[256];
				sprintf_s(buf, sizeof(buf), "Octave DOWN set to: %s - Now press keybind for OCTAVE UP", combo_str);
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
				text_needs_update = 1;
				
				OutputDebugStringA("Octave DOWN key set, moving to octave UP\n");
				app_mode = MODE_REBINDING_OCTAVE_UP;
				return 0;
			}
			
			if (app_mode == MODE_REBINDING_OCTAVE_UP) {
				if (w_param == VK_SHIFT || w_param == VK_CONTROL || 
					w_param == VK_MENU || w_param == VK_LSHIFT || 
					w_param == VK_RSHIFT || w_param == VK_LCONTROL || 
					w_param == VK_RCONTROL || w_param == VK_LMENU ||
					w_param == VK_RMENU) {
					return 0;
				}
				
				//Ignore F6-F11
				if (w_param >= VK_F6 && w_param <= VK_F11) {
					return 0;
				}
				
				WORD scancode = (WORD)((l_param >> 16) & 0x1FF);
				octave_up_combo.scancode = scancode;
				octave_up_combo.use_shift = shift_pressed;
				octave_up_combo.use_ctrl = ctrl_pressed;
				octave_up_combo.use_alt = alt_pressed;
				
				char ch = (char)MapVirtualKeyA(w_param, MAPVK_VK_TO_CHAR);
				if (ch < 32 || ch >= 127) ch = '?';
				
				char combo_str[128];
				get_combo_display_string(&octave_up_combo, ch, combo_str, sizeof(combo_str));
				
				char buf[256];
				sprintf_s(buf, sizeof(buf), "Octave UP set to: %s - Now press MIDI keys to rebind notes (F8 to exit)", combo_str);
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
				text_needs_update = 1;
				
				OutputDebugStringA("Octave UP key set, moving to note rebinding\n");
				app_mode = MODE_REBINDING_NOTES;
				return 0;
			}

			if (app_mode == MODE_REBINDING_NOTES && rebind_target_note != -1) {
				if (w_param == VK_SHIFT || w_param == VK_CONTROL || 
					w_param == VK_MENU || w_param == VK_LSHIFT || 
					w_param == VK_RSHIFT || w_param == VK_LCONTROL || 
					w_param == VK_RCONTROL || w_param == VK_LMENU ||
					w_param == VK_RMENU) {
					return 0;
				}
				
				//Ignore F6-F11
				if (w_param >= VK_F6 && w_param <= VK_F11) {
					return 0;
				}
				
				WORD scancode = (WORD)((l_param >> 16) & 0x1FF);
				
				int starter_note = cstarter_note();
				int offset = rebind_target_note - starter_note;
				
				if (offset >= 0 && offset < 13) {
					key_map[offset].scancode = scancode;
					key_map[offset].use_shift = shift_pressed;
					key_map[offset].use_ctrl = ctrl_pressed;
					key_map[offset].use_alt = alt_pressed;
					
					char ch = (char)MapVirtualKeyA(w_param, MAPVK_VK_TO_CHAR);
					if (ch >= 32 && ch < 127) {
						display_map[offset] = ch;
					} else {
						display_map[offset] = '?';
					}
					
					char combo_str[128];
					get_combo_display_string(&key_map[offset], display_map[offset], 
											combo_str, sizeof(combo_str));
					
					char buf[256];
					sprintf_s(buf, sizeof(buf), 
							"Bound MIDI note %d to: %s (scancode %d)\n",
							rebind_target_note, combo_str, scancode);
					OutputDebugStringA(buf);
					
					sprintf_s(buf, sizeof(buf), 
							"Saved: %s - Press another MIDI key or F8 to exit", combo_str);
					strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
					text_needs_update = 1;
				}
				
				rebind_target_note = -1;
				return 0;
			}

			//F9 to copy text to clipboard
			if (w_param == VK_F9 && rebind_target_note == -1) {
				copy_to_clipboard(midi_key_display);
				OutputDebugStringA("Text copied to clipboard!\n");
				return 0;
			}

			//F11 to restore defaults
    		if (w_param == VK_F11 && rebind_target_note == -1) {
                clear_display();
				restore_default_keybindings();
				strcpy_s(midi_key_display, MAX_INPUT_LENGTH, "Restored to default keybindings!");
				text_needs_update = 1;
				return 0;
			}

			
		}break;

		case WM_KEYUP: {

		} break;

		case WM_DESTROY:
		{
			OutputDebugStringA("WM_DESTROY\n");
			PostQuitMessage(0);
		}break;

		case WM_CLOSE:
		{
			OutputDebugStringA("WM_CLOSE\n");
			DestroyWindow(window_handle);
		}break;

		case WM_ACTIVATEAPP:
		{
			OutputDebugStringA("WM_ACTIVATEAPP\n");
		}break;

		case WM_PAINT:
		{

		}break;

		default:
		{
			Result = DefWindowProcA(window_handle,message,w_param,l_param);
		}break;
	}
	return Result;

}

int is_black(int semitone) 
	{
        return (semitone == 1 || semitone == 3 ||
                semitone == 6 || semitone == 8 ||
                semitone == 10);
    }

void init_keys()
{
    const int TOTAL_KEYS = 88;
    const int WHITE_KEYS_COUNT = 52;

    float white_w_px = 116.0f;
    float white_h_px = 670.0f;
    float white_start_x = 359.0f;
    float white_start_y = 1118.0f;

    float black_w_px = 78.0f;
    float black_h_px = 422.0f;
    
    int black_pattern[7] = {1, 1, 0, 1, 1, 1, 0};

    for (int i = 0; i < WHITE_KEYS_COUNT-1; ++i) {
        PianoKey *k = &piano_keys[i];
        k->tex_x = white_start_x + i * white_w_px;
        k->tex_y = white_start_y;
        k->tex_w = white_w_px;
        k->tex_h = white_h_px;
        k->is_black = 0;
        k->is_pressed = 0;
        k->midi_note = -1;
    }

    int idx = WHITE_KEYS_COUNT;
    int step = 5;
    
    for (int i = 0; i < WHITE_KEYS_COUNT - 1; ++i) {
        if (black_pattern[step]) {
            PianoKey *k = &piano_keys[idx++];
            float bx = white_start_x + i * white_w_px + (white_w_px * 0.65f);
            k->tex_x = bx;
            k->tex_y = white_start_y;
            k->tex_w = black_w_px;
            k->tex_h = black_h_px;
            k->is_black = 1;
            k->is_pressed = 0;
            k->midi_note = -1;
        }
        step = (step + 1) % 7;
    }

    int white_idx = 0;
    int black_idx = WHITE_KEYS_COUNT;

    for (int m = 21; m <= 108; ++m) {
        int sem = m % 12;
        if (is_black(sem)) {
            if (black_idx < TOTAL_KEYS)
                piano_keys[black_idx++].midi_note = m;
        } else {
            if (white_idx < WHITE_KEYS_COUNT)
                piano_keys[white_idx++].midi_note = m;
        }
    }
}

void convert_tex_rect_to_window(
    float tex_x, float tex_y, float tex_w_px, float tex_h_px,
    float frame_tex_w, float frame_tex_h,
    float screen_w, float screen_h,
    float *out_x, float *out_y, float *out_w, float *out_h)
{
    float tex_aspect = frame_tex_w / frame_tex_h;
    float win_aspect = screen_w / screen_h;

    float quad_w = 1.0f, quad_h = 1.0f;
    if (tex_aspect > win_aspect) {
        quad_w = 1.0f;
        quad_h = win_aspect / tex_aspect;
    } else {
        quad_h = 1.0f;
        quad_w = tex_aspect / win_aspect;
    }

    float frame_display_w = quad_w * screen_w * 0.5f;
    float frame_display_h = quad_h * screen_h * 0.5f;
    float frame_x = (screen_w - 2 * frame_display_w) * 0.5f;
    float frame_y = (screen_h - 2 * frame_display_h) * 0.5f;

    float sx = tex_x / frame_tex_w;
    float sy = tex_y / frame_tex_h;
    float sw = tex_w_px / frame_tex_w;
    float sh = tex_h_px / frame_tex_h;

    *out_x = frame_x + sx * (2.0f * frame_display_w);
    *out_y = frame_y + sy * (2.0f * frame_display_h);
    *out_w = sw * (2.0f * frame_display_w);
    *out_h = sh * (2.0f * frame_display_h);
}

void CALLBACK midi_callback(HMIDIIN hMidiIn, UINT wMsg,
                            DWORD_PTR dwInstance,
                            DWORD_PTR msg, DWORD_PTR timestamp, allocator_i *allocator) 
{
    if (wMsg != MIM_DATA)
        return;

    DWORD data = (DWORD)msg;
    int status = data & 0xF0; 
    int note   = (data >> 8) & 0x7F;
    int vel    = (data >> 16) & 0x7F;

    char buf[256];
    int starter_note = cstarter_note();
    
    if (status == 0x90 && vel > 0)
    {
        // REBINDING MODE 
        if (app_mode == MODE_REBINDING_NOTES) {
            rebind_target_note = note;
            
            int offset = note - starter_note;
            sprintf_s(buf, sizeof(buf), 
                     "MIDI note %d (offset %d) - Press keybind...", 
                     note, offset);
            strcpy_s(midi_key_display, MAX_INPUT_LENGTH, buf);
            text_needs_update = 1;
            
            OutputDebugStringA(buf);
            OutputDebugStringA("\n");
            return;
        }
        
        if (app_mode != MODE_NORMAL) {
            return;
        }
        
        bool octave_shifted = false;

        // NORMAL MODE
        if(note >= starter_note + 13) {
            shift_octave(true);
			octave_shifted = true;
        }
        else if(note <= starter_note - 1) {
            shift_octave(false);
			octave_shifted = true;
        }

		 if (octave_shifted) {
			if(get_gw2_ping(allocator, &gw2_ping))
			{
				Sleep(gw2_ping);
			}
			else
			{
            	Sleep(octave_shift_delay); 
			}
            starter_note = cstarter_note(); 
        }
        
        key_pressed_note[note] = 1;
        
        KeyCombo *combo = midi_note_to_key_combo(note, starter_note);
        
        if (combo == NULL) {
            return;
        }
        
        //Release modifiers
        INPUT release_mods[3] = {0};
        int mod_count = 0;
        
        //Shift
        if (!combo->use_shift) {
            release_mods[mod_count].type = INPUT_KEYBOARD;
            release_mods[mod_count].ki.wScan = 42;
            release_mods[mod_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            mod_count++;
        }
        
        //Ctrl
        if (!combo->use_ctrl) {
            release_mods[mod_count].type = INPUT_KEYBOARD;
            release_mods[mod_count].ki.wScan = 29;
            release_mods[mod_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            mod_count++;
        }
        
        //Alt
        if (!combo->use_alt) {
            release_mods[mod_count].type = INPUT_KEYBOARD;
            release_mods[mod_count].ki.wScan = 56;
            release_mods[mod_count].ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;
            mod_count++;
        }

        //Releases	
        if (mod_count > 0) {
            SendInput(mod_count, release_mods, sizeof(INPUT));
        }
        press_key_combo(combo);
		note_combo_map[note] = *combo;
        
        char display_char = midi_note_to_display_char(note, starter_note);
        char combo_str[128];
        get_combo_display_string(combo, display_char, combo_str, sizeof(combo_str));
        
        //Update piano visual
        for (int i = 0; i < NUM_KEYS; i++) {
            if (piano_keys[i].midi_note == note) {
                piano_keys[i].is_pressed = 1;
                break;
            }
        }
        
        if (midi_display_length < MAX_INPUT_LENGTH - 20) {
            sprintf_s(buf, sizeof(buf), "%s ", combo_str);
            strcat_s(midi_key_display, MAX_INPUT_LENGTH, buf);
            midi_display_length = (int)strlen(midi_key_display);

            //Auto scroll
            if (midi_display_length > viewport_start + viewport_max_chars) {
                viewport_start = midi_display_length - viewport_max_chars;
            }

            text_needs_update = 1;
        }
    }
    
    if(status == 0x80) {
        if (app_mode != MODE_NORMAL) {
            return;
        }
        
        key_pressed_note[note] = 0;
         KeyCombo *combo = &note_combo_map[note];

        if (combo == NULL) {
            return;
        }
        
        release_key_combo(combo);
        
        for (int i = 0; i < NUM_KEYS; i++) {
            if (piano_keys[i].midi_note == note) {
                piano_keys[i].is_pressed = 0;
                break;
            }
        }

		note_combo_map[note].scancode = 0;
		note_combo_map[note].use_shift = false;
		note_combo_map[note].use_ctrl = false;
		note_combo_map[note].use_alt = false;
    }
}


void init_midi()
{
    HMIDIIN midi_in;

    UINT count = midiInGetNumDevs();
    if (count == 0) {
        OutputDebugStringA("No MIDI devices found.\n");
        return;
    }

    MMRESULT result = midiInOpen(
        &midi_in,
        0,
        (DWORD_PTR)midi_callback,
        0,
        CALLBACK_FUNCTION
    );

    if (result != MMSYSERR_NOERROR) {
        OutputDebugStringA("Failed to open MIDI device.\n");
        return;
    }

    midiInStart(midi_in);
    OutputDebugStringA("MIDI listening...\n");
}


int CALLBACK WinMain(
HINSTANCE hInstance,
HINSTANCE hPrevInstance,
LPSTR     lpCmdLine,
int       nShowCmd)
{
	tagWNDCLASSA WindowClass = {};
	WindowClass.style = CS_OWNDC|CS_HREDRAW|CS_VREDRAW;
	WindowClass.lpfnWndProc = Wndproc;
	WindowClass.hInstance = hInstance;
	WindowClass.lpszClassName = "GW2VisualMIDIClass";

	RegisterClassA(&WindowClass);

	POINT pt;
	GetCursorPos(&pt);

	HMONITOR mon = MonitorFromPoint(pt, MONITOR_DEFAULTTONEAREST);

	MONITORINFO mi = {0};
	mi.cbSize = sizeof(mi);
	GetMonitorInfo(mon, &mi);

	int monitor_w = mi.rcMonitor.right - mi.rcMonitor.left;

	int client_w = monitor_w;
	int client_h = (int)(1903.0f / 6613.0f * client_w);
	DWORD style = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX;
	DWORD exStyle = 0;
	RECT r = {0, 0, client_w, client_h};
	AdjustWindowRectEx(&r, style, FALSE, exStyle);

	int win_w = r.right - r.left;
	int win_h = r.bottom - r.top;

	HWND window_handle = CreateWindowExA(
	0,
    WindowClass.lpszClassName,
	"GW2VisualMIDI",
	style | WS_VISIBLE,
	CW_USEDEFAULT,
	CW_USEDEFAULT,
	win_w,
	win_h,
	0,
	0,
	hInstance, 
	0);

	allocator_i *allocator = get_simple_allocator();

    input_text = (char*)allocator->realloc(allocator->inst, NULL, MAX_INPUT_LENGTH, 0);
    midi_key_display = (char*)allocator->realloc(allocator->inst, NULL, MAX_INPUT_LENGTH, 0);
    
    if (!input_text || !midi_key_display) {
        MessageBoxA(NULL, "Failed to allocate text buffers", "Error", MB_OK);
        return 1;
    }
    
    input_text[0] = '\0';
    midi_key_display[0] = '\0';
	
	RECT rect;
	GetClientRect(window_handle, &rect);
	float screen_width = rect.right - rect.left;
	float screen_height = rect.bottom - rect.top;
	
	
	float tex_w = 6613.0f;
	float tex_h = 1903.0f;

	float frame_x, frame_y, frame_w, frame_h;

	convert_tex_rect_to_window(
		0, 0,
		tex_w, tex_h,
		tex_w, tex_h,
		screen_width,
		screen_height,
		&frame_x, &frame_y,
		&frame_w, &frame_h
	);

	float frame_vertices[30];

	set_quad_vertices(
		frame_vertices,
		frame_x, frame_y,
		frame_w, frame_h,
		screen_width, screen_height
	);

	HDC device_context = wglGetCurrentDC();

	//Frame VAO/VBO
	GLuint vao_frame, vbo_frame;
	glGenVertexArrays(1, &vao_frame);
	glGenBuffers(1, &vbo_frame);

	glBindVertexArray(vao_frame);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_frame);
	glBufferData(GL_ARRAY_BUFFER, sizeof(frame_vertices), frame_vertices, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	//Key VAO/VBO
	float key_vertices[30];
	GLuint vao_key, vbo_key;
	glGenVertexArrays(1, &vao_key);
	glGenBuffers(1, &vbo_key);

	glBindVertexArray(vao_key);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_key);
	glBufferData(GL_ARRAY_BUFFER, sizeof(key_vertices), key_vertices, GL_DYNAMIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);

	//Font VAO/VBO

	float text_verts[5 * 6 * 256];

	GLuint vao_font, vbo_font;
	glGenVertexArrays(1, &vao_font);
	glGenBuffers(1, &vbo_font);

	glBindVertexArray(vao_font);
	glBindBuffer(GL_ARRAY_BUFFER, vbo_font);
	glBufferData(GL_ARRAY_BUFFER, sizeof(text_verts), NULL, GL_STATIC_DRAW);
	glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), 0);
	glEnableVertexAttribArray(0);
	glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	glEnableVertexAttribArray(1);
	
	//Compile Shaders
	GLuint vertex_shader = compile_shader("Assets/Shaders/vertex_shader.glsl", GL_VERTEX_SHADER,allocator);
	GLuint fragment_shader = compile_shader("Assets/Shaders/fragment_shader.glsl", GL_FRAGMENT_SHADER, allocator);
	GLuint font_vert = compile_shader("Assets/Shaders/font_vert.glsl", GL_VERTEX_SHADER, allocator);
	GLuint font_frag = compile_shader("Assets/Shaders/font_frag.glsl", GL_FRAGMENT_SHADER, allocator);

	//Shader program setup
	GLuint shader_program = glCreateProgram();
	glAttachShader(shader_program, vertex_shader);
	glAttachShader(shader_program, fragment_shader);
	glLinkProgram(shader_program);

	GLint success;
	glGetProgramiv(shader_program, GL_VALIDATE_STATUS, &success);
	if (!success) {
		char info_log[512];
		glGetProgramInfoLog(shader_program, 512, NULL, info_log);
		OutputDebugStringA(info_log);
	}
		
	glDeleteShader(vertex_shader);
	glDeleteShader(fragment_shader);

	//Font Shader Program setup
	GLuint font_shader_program = glCreateProgram();
	glAttachShader(font_shader_program,font_vert);
	glAttachShader(font_shader_program,font_frag);
	glLinkProgram(font_shader_program);
	glGetProgramiv(font_shader_program, GL_VALIDATE_STATUS, &success);
	if (!success) {
		char info_log[512];
		glGetProgramInfoLog(font_shader_program, 512, NULL, info_log);
		OutputDebugStringA(info_log);
	}

	glDeleteShader(font_vert);
	glDeleteShader(font_frag);
	

	//Textures
	GLuint frame_tex = load_texture("Assets/Piano_Assets/Piano_Body.png");
	GLuint white_unpressed_tex = load_texture("Assets/Piano_Assets/White_Unpressed.png");
	GLuint white_pressed_tex   = load_texture("Assets/Piano_Assets/White_Pressed.png");
	GLuint black_unpressed_tex = load_texture("Assets/Piano_Assets/Black_Unpressed.png");
	GLuint black_pressed_tex = load_texture("Assets/Piano_Assets/Black_Pressed.png");

	//Font Textures
	GLuint font_tex = load_texture("Assets/Fonts/atlas.png");
	if (font_tex == 0) {
		OutputDebugStringA("ERROR: Failed to load font texture!\n");
	}

	//Ping Measure init
	get_gw2_ping(allocator,&gw2_ping);

	init_keys();
	init_midi();
	load_keybindings("keybindings.txt");

	parse_json("Assets/Fonts/atlas.json");
	if (glyphs['H'].advance == 0.0f) {
    	OutputDebugStringA("ERROR: Glyphs not loaded!\n");
	}
	
	int text_vert_count = 0;

	MSG message;
	int running = 1; 

	while(running)
	{
		while(PeekMessageA(&message, NULL, 0, 0, PM_REMOVE))
		{
			if (message.message == WM_QUIT)
			{
                allocator->realloc(allocator->inst, input_text, 0, MAX_INPUT_LENGTH); //Free text buffers
                allocator->realloc(allocator->inst, midi_key_display, 0, MAX_INPUT_LENGTH);
				allocator->realloc(allocator->inst, tcp_table, 0, tcp_table_capacity); //Free TCP Table
				uint64_t total_allocated = allocator->get_total_allocated(allocator->inst);
				if(total_allocated != 0)
				{
				fprintf(stderr, "Assertion failed: Memory leak detected! total_allocated = %d\n", total_allocated);
				assert(total_allocated == 0 );
				}
				running = 0;
			}
			else
			{
				//Message Loop
				TranslateMessage(&message);
				DispatchMessageW(&message);
				
				
				//Render Loop

				glClear(GL_COLOR_BUFFER_BIT);  	 
				
				get_gw2_ping(allocator, &gw2_ping);
				//Get iResolution uniform
				RECT rect;
				GetClientRect(window_handle, &rect);
				int screen_width = rect.right - rect.left;
				int screen_height = rect.bottom -   rect.top;
				
				glClear(GL_COLOR_BUFFER_BIT); 
				glUseProgram(shader_program);
				GLint texLoc = glGetUniformLocation(shader_program, "tex0");
				glUniform1i(texLoc, 0);
				//Frame
				glBindTexture(GL_TEXTURE_2D, frame_tex);
				glBindVertexArray(vao_frame);
				glDrawArrays(GL_TRIANGLES, 0, 6);

				glBindVertexArray(vao_key);
				for (int i = 0; i < NUM_WHITE_KEYS; ++i) { 
					PianoKey *k = &piano_keys[i];

					float x, y, w, h;
					convert_tex_rect_to_window(k->tex_x, k->tex_y, k->tex_w, k->tex_h,
											   6613.0f, 1903.0f,
											   (float)screen_width, (float)screen_height,
											   &x, &y, &w, &h);

					set_quad_vertices(key_vertices, x, y, w, h, (float)screen_width, (float)screen_height);
					glBindBuffer(GL_ARRAY_BUFFER, vbo_key);
					glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(key_vertices), key_vertices);

					GLuint tex = k->is_pressed ? white_pressed_tex : white_unpressed_tex;
					glBindTexture(GL_TEXTURE_2D, tex);
					glDrawArrays(GL_TRIANGLES, 0, 6);
				}

				for (int i = NUM_WHITE_KEYS; i < NUM_KEYS; ++i) {
					PianoKey *k = &piano_keys[i];

					float x, y, w, h;
					convert_tex_rect_to_window(k->tex_x, k->tex_y, k->tex_w, k->tex_h,
											   6613.0f, 1903.0f,
											   (float)screen_width, (float)screen_height,
											   &x, &y, &w, &h);

					set_quad_vertices(key_vertices, x, y, w, h, (float)screen_width, (float)screen_height);
					glBindBuffer(GL_ARRAY_BUFFER, vbo_key);
					glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(key_vertices), key_vertices);

					GLuint tex = k->is_pressed ? black_pressed_tex : black_unpressed_tex;
					glBindTexture(GL_TEXTURE_2D, tex);
					glDrawArrays(GL_TRIANGLES, 0, 6);
				}

				if(text_needs_update)
				{
					char visible_text[256];
					
					get_visible_text(midi_key_display, viewport_start, viewport_max_chars, 
									visible_text, sizeof(visible_text));
									
					text_vert_count = 0;
					build_text_quads(
						visible_text,
						screen_width/7.0f,
						screen_height/2.5f,
						40.0f,
						text_verts,
						&text_vert_count,
						screen_width,
						screen_height   
					);
					text_needs_update = 0;
				};

				glEnable(GL_BLEND);
				glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

				glUseProgram(font_shader_program);

				glActiveTexture(GL_TEXTURE0);
				glBindTexture(GL_TEXTURE_2D, font_tex);
				glUniform1i(glGetUniformLocation(font_shader_program, "uFont"), 0);

				glUniform4f(
					glGetUniformLocation(font_shader_program, "uColor"),
					1.0f, 1.0f, 1.0f, 1.0f
				);

				glBindVertexArray(vao_font);

				glBindBuffer(GL_ARRAY_BUFFER, vbo_font);
				glBufferSubData(
					GL_ARRAY_BUFFER,
					0,
					text_vert_count * 5 * sizeof(float),
					text_verts
				);

				//Draw
				glDrawArrays(GL_TRIANGLES, 0, text_vert_count);
				SwapBuffers(device_context); 
			}
		}
	}
 
	return 0;
}
