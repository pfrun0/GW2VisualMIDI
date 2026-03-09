#include <stdio.h>
#include <stdlib.h>
#include <windows.h>
#include <GLAD/glad.h> 
#include "allocator.h"

GLuint compile_shader(const char* filestr, GLenum type, allocator_i *allocator) {
	if(strncmp(filestr, "#version", 8) == 0)
	{
		//filestr is the shader in this case
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &filestr, NULL);
		glCompileShader(shader);

		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, NULL, infoLog);
			OutputDebugStringA(infoLog);
		}
	}
	else
	{
		//filestr is the file name in this case
		FILE* file = fopen(filestr, "rb"); 

		fseek(file, 0, SEEK_END);
		long length = ftell(file);
		rewind(file);

		char *source = (char*)allocator->realloc(allocator->inst, NULL, length + 1,0);
 
		fread(source, 1, length, file);
		source[length] = '\0'; 
		fclose(file);
		
		GLuint shader = glCreateShader(type);
		glShaderSource(shader, 1, &source, NULL);
		glCompileShader(shader);

		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			char infoLog[512];
			glGetShaderInfoLog(shader, 512, NULL, infoLog);
			OutputDebugStringA(infoLog);
		}
		allocator->realloc(allocator->inst, source, 0,length+1);
		return shader;
	}
	
}
	 