#include "angle_trace_gl.h"
#include "CapturedTest_ExternalEGLSync_ES3_Vulkan.h"

const char *const glShaderSource_string_0[] = { 
"attribute vec4 a_position;\n"
"varying vec2 v_texCoord;\n"
"void main()\n"
"{\n"
"    gl_Position = a_position;\n"
"    v_texCoord = a_position.xy * 0.5 + 0.5;\n"
"}",
};
const char *const glShaderSource_string_1[] = { 
"precision mediump float;\n"
"varying vec2 v_texCoord;\n"
"uniform sampler2D s_texture;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D(s_texture, v_texCoord);\n"
"}",
};

// Private Functions

void SetupReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, (GLuint *)gReadBuffer);
    UpdateTextureID(1, 0);
    glBindTexture(GL_TEXTURE_2D, gTextureMap[1]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 9728);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 9728);
    glTexImage2D(GL_TEXTURE_2D, 0, 6408, 2, 2, 0, GL_RGBA, GL_UNSIGNED_BYTE, (const GLubyte *)GetBinaryData(160));
    CreateProgram(1);
    CreateShader(GL_VERTEX_SHADER, 4);
    glShaderSource(gShaderProgramMap[4], 1, glShaderSource_string_0, 0);
    glCompileShader(gShaderProgramMap[4]);
    glAttachShader(gShaderProgramMap[1], gShaderProgramMap[4]);
    CreateShader(GL_FRAGMENT_SHADER, 5);
    glShaderSource(gShaderProgramMap[5], 1, glShaderSource_string_1, 0);
    glCompileShader(gShaderProgramMap[5]);
    glAttachShader(gShaderProgramMap[1], gShaderProgramMap[5]);
    glBindAttribLocation(gShaderProgramMap[1], 0, "a_position");
    glLinkProgram(gShaderProgramMap[1]);
    UpdateUniformLocation(1, "s_texture", 0, 1);
    glUseProgram(gShaderProgramMap[1]);
    UpdateCurrentProgramPerContext(1);
    glUniform1iv(gUniformLocations[gCurrentProgramPerContext[gCurrentContext]][0], 1, (const GLint *)GetBinaryData(176));
    glDeleteShader(gShaderProgramMap[4]);
    glDeleteShader(gShaderProgramMap[5]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void SetupReplayContextSharedInactive(void)
{
}

// Public Functions

