#include "fonts.h"
#include "piano_keys.h"
#include "cJSON.h"
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

sp_glyph_t glyphs[256];

static char *load_file(const char *path) {
    FILE *f = NULL;
    errno_t err = fopen_s(&f, path, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *buffer = (char*)malloc(size + 1);
    fread(buffer, 1, size, f);
    buffer[size] = '\0';

    fclose(f);
    return buffer;
}

int parse_json(const char *path) {
    char *text = load_file(path);
    if (!text) {
        OutputDebugStringA("Failed to load JSON file\n");
        return 0;
    }

    cJSON *root = cJSON_Parse(text);
    free(text);
    if (!root) {
        OutputDebugStringA("Failed to parse JSON!\n");
        return 0;
    }

    cJSON *glyphs_array = cJSON_GetObjectItem(root, "glyphs");
    if (!glyphs_array || !cJSON_IsArray(glyphs_array)) {
        OutputDebugStringA("Missing or invalid 'glyphs' array\n");
        cJSON_Delete(root);
        return 0;
    }

    cJSON *entry = NULL;
    cJSON_ArrayForEach(entry, glyphs_array) {
        cJSON *unicode = cJSON_GetObjectItem(entry, "unicode");
        cJSON *advance = cJSON_GetObjectItem(entry, "advance");
        cJSON *plane   = cJSON_GetObjectItem(entry, "planeBounds");
        cJSON *atlas   = cJSON_GetObjectItem(entry, "atlasBounds");

        if (!unicode || !advance || !plane || !atlas) continue;
        unsigned char ch = (unsigned char) unicode->valueint;
        sp_glyph_t *g = &glyphs[ch];

        g->advance      = (float) advance->valuedouble;

        g->plane_left   = (float) cJSON_GetObjectItem(plane, "left")->valuedouble;
        g->plane_bottom = (float) cJSON_GetObjectItem(plane, "bottom")->valuedouble;
        g->plane_right  = (float) cJSON_GetObjectItem(plane, "right")->valuedouble;
        g->plane_top    = (float) cJSON_GetObjectItem(plane, "top")->valuedouble;

        g->atlas_left   = (float) cJSON_GetObjectItem(atlas, "left")->valuedouble;
        g->atlas_bottom = (float) cJSON_GetObjectItem(atlas, "bottom")->valuedouble;
        g->atlas_right  = (float) cJSON_GetObjectItem(atlas, "right")->valuedouble;
        g->atlas_top    = (float) cJSON_GetObjectItem(atlas, "top")->valuedouble;
    }

    cJSON_Delete(root);
    return 1;
}

void build_text_quads(
    const char *text,
    float start_x,
    float baseline_y,
    float font_size,
    float *out_verts,
    int *out_vert_count,
    float window_w,
    float window_h)
{
    const float atlas_w = 1024.0f;
    const float atlas_h = 1024.0f;

    float cursor_x = start_x;
    float cursor_y = baseline_y;

    int vert_index = 0;

    for (const char *p = text; *p; p++) {
        unsigned char c = (unsigned char)*p;

        if (c == ' ') {
            cursor_x += font_size * 0.33f;
            continue;
        }

        sp_glyph_t *g = &glyphs[c];

        if (g->advance == 0.0f)
            continue;

        float x = cursor_x + g->plane_left * font_size;
        float y = cursor_y - g->plane_top  * font_size;

        float w = (g->plane_right - g->plane_left) * font_size;
        float h = (g->plane_top   - g->plane_bottom) * font_size;

        float u0 = g->atlas_left   / atlas_w;
        float v0 = 1.0f - (g->atlas_bottom / atlas_h); 
        float u1 = g->atlas_right  / atlas_w;
        float v1 = 1.0f - (g->atlas_top / atlas_h);  

        set_glyph_quad_vertices(
            &out_verts[vert_index],
            x, y, w, h,
            window_w, window_h,
            u0, v0, u1, v1
        );

        vert_index += 30;

        cursor_x += g->advance * font_size;
    }

    *out_vert_count = vert_index / 5;
}

void get_visible_text(const char *full_text, int start_index, int max_chars, 
                     char *out_buffer, int buffer_size) 
{
    int text_len = (int)strlen(full_text);
    
    if (start_index < 0) start_index = 0;
    if (start_index >= text_len) start_index = text_len;
    
    int chars_available = text_len - start_index;
    int chars_to_show = (chars_available < max_chars) ? chars_available : max_chars;
    
    if (chars_to_show > 0) {
        strncpy_s(out_buffer, buffer_size, full_text + start_index, chars_to_show);
        out_buffer[chars_to_show] = '\0';
    } else {
        out_buffer[0] = '\0';
    }
}

void copy_to_clipboard(const char *text) 
{
    if (!OpenClipboard(NULL)) {
        OutputDebugStringA("Failed to open clipboard\n");
        return;
    }
    
    EmptyClipboard();
    
    size_t len = strlen(text) + 1;
    HGLOBAL hMem = GlobalAlloc(GMEM_MOVEABLE, len);
    if (hMem) {
        char *pMem = (char*)GlobalLock(hMem);
        if (pMem) {
            strcpy_s(pMem, len, text);
            GlobalUnlock(hMem);
            SetClipboardData(CF_TEXT, hMem);
        }
    }
    
    CloseClipboard();
}