#pragma once
#include <glad/glad.h>
#include <allocator.h>

#ifdef __cplusplus
extern "C" {
#endif

GLuint compile_shader(const char* file, GLenum type, allocator_i *allocator);

#ifdef __cplusplus
}
#endif
