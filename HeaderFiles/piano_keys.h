#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#ifndef PIANO_KEYS_H
#define PIANO_KEYS_H

#include <GLAD/glad.h>

void set_quad_vertices(float *out,
                       float x, float y,
                       float w, float h,
                       float win_w, float win_h);

#endif
void set_glyph_quad_vertices(
    float *out_vertices,
    float x, float y,
    float w, float h,
    float window_w, float window_h,
    float u0, float v0,
    float u1, float v1);



#ifdef __cplusplus
}
#endif
