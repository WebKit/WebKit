#include "angle_trace_gl.h"
#include "CapturedTest_MultiFrame_ES3_Vulkan.h"

const char *const glShaderSource_string_4[] = { 
"precision highp float;\n"
"void main(void) {\n"
"   gl_Position = vec4(0.5, 0.5, 0.5, 1.0);\n"
"}",
};

// Private Functions

void SetupReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, (GLuint *)gReadBuffer);
    UpdateTextureID(2, 0);
    glBindTexture(GL_TEXTURE_2D, gTextureMap[2]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 9728);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 9728);
    glTexImage2D(GL_TEXTURE_2D, 0, 6407, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, (const GLubyte *)&gBinaryData[464]);
    CreateProgram(3);
    CreateShader(GL_VERTEX_SHADER, 6);
    glShaderSource(gShaderProgramMap[6], 1, glShaderSource_string_0, 0);
    glCompileShader(gShaderProgramMap[6]);
    glAttachShader(gShaderProgramMap[3], gShaderProgramMap[6]);
    CreateShader(GL_FRAGMENT_SHADER, 7);
    glShaderSource(gShaderProgramMap[7], 1, glShaderSource_string_1, 0);
    glCompileShader(gShaderProgramMap[7]);
    glAttachShader(gShaderProgramMap[3], gShaderProgramMap[7]);
    glBindAttribLocation(gShaderProgramMap[3], 0, "a_position");
    glBindAttribLocation(gShaderProgramMap[3], 1, "a_texCoord");
    glLinkProgram(gShaderProgramMap[3]);
    UpdateUniformLocation(3, "s_texture", 0, 1);
    glUseProgram(gShaderProgramMap[3]);
    UpdateCurrentProgram(3);
    glUniform1iv(gUniformLocations[gCurrentProgram][0], 1, (const GLint *)&gBinaryData[480]);
    glDeleteShader(gShaderProgramMap[6]);
    glDeleteShader(gShaderProgramMap[7]);
}

void SetupReplayContextSharedInactive(void)
{
    CreateShader(GL_VERTEX_SHADER, 2);
    glShaderSource(gShaderProgramMap[2], 1, glShaderSource_string_4, 0);
    glCompileShader(gShaderProgramMap[2]);
    CreateShader(GL_VERTEX_SHADER, 4);
    glShaderSource(gShaderProgramMap[4], 1, glShaderSource_string_0, 0);
    glCompileShader(gShaderProgramMap[4]);
    CreateShader(GL_FRAGMENT_SHADER, 5);
    glShaderSource(gShaderProgramMap[5], 1, glShaderSource_string_1, 0);
    glCompileShader(gShaderProgramMap[5]);
}

// Public Functions

