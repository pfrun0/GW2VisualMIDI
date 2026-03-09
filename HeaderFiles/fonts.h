#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <windows.h>

typedef struct {
    float advance;

    float plane_left;
    float plane_bottom;
    float plane_right;
    float plane_top;

    float atlas_left;
    float atlas_bottom;
    float atlas_right;
    float atlas_top;
} sp_glyph_t;

int parse_json(const char *path);

void build_text_quads(
    const char *text,
    float start_x,
    float baseline_y,
    float font_size,
    float *out_verts,
    int *out_vert_count,
    float window_w,
    float window_h);

void get_visible_text(
    const char *full_text, 
    int start_index, 
    int max_chars, 
    char *out_buffer, 
    int buffer_size);

void copy_to_clipboard(const char *text);

extern sp_glyph_t glyphs[256];


#ifdef __cplusplus
}
#endif
