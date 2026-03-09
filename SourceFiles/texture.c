#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"
#include <GLAD/glad.h>

GLuint load_texture(const char* path) {
    int w,h,comp;
    unsigned char* data = stbi_load(path,&w,&h,&comp,4);
    if(!data) return 0;

    GLuint tex;
    glGenTextures(1,&tex);
    glBindTexture(GL_TEXTURE_2D,tex);
    glTexImage2D(GL_TEXTURE_2D,0,GL_RGBA8,w,h,0,GL_RGBA,GL_UNSIGNED_BYTE,data);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);

    stbi_image_free(data);
    return tex;
}
