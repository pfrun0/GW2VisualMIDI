#include "piano_keys.h"
#include "texture.h"
#include <string.h>

void set_quad_vertices(float *out_vertices,
                       float x, float y,
                       float w, float h,
                       float window_w, float window_h)
{
    float left   =  (x / window_w) * 2.0f - 1.0f;
    float right  = ((x + w) / window_w) * 2.0f - 1.0f;

    float top    =  1.0f - (y / window_h) * 2.0f;
    float bottom =  1.0f - ((y + h) / window_h) * 2.0f;

    // x, y, z, u, v
    float verts[30] = {
        left,  top,    0, 0, 1,
        left,  bottom, 0, 0, 0,
        right, top,    0, 1, 1,

        left,  bottom, 0, 0, 0,
        right, bottom, 0, 1, 0,
        right, top,    0, 1, 1
    };

    memcpy(out_vertices, verts, sizeof(verts));
}

void set_glyph_quad_vertices(
    float *out_vertices,
    float x, float y,
    float w, float h,
    float window_w, float window_h,
    float u0, float v0,
    float u1, float v1)
{
    float left   =  (x / window_w) * 2.0f - 1.0f;
    float right  = ((x + w) / window_w) * 2.0f - 1.0f;

    float top    =  1.0f - (y / window_h) * 2.0f;
    float bottom =  1.0f - ((y + h) / window_h) * 2.0f;

    float verts[30] = {
        left,  top,    0, u0, v1,
        left,  bottom, 0, u0, v0,
        right, top,    0, u1, v1,

        left,  bottom, 0, u0, v0,
        right, bottom, 0, u1, v0,
        right, top,    0, u1, v1
    };

    memcpy(out_vertices, verts, sizeof(verts));
}