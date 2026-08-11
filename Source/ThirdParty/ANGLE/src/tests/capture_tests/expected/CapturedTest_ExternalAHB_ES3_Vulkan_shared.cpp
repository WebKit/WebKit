#include "angle_trace_gl.h"
#include "CapturedTest_ExternalAHB_ES3_Vulkan.h"

const char *const glShaderSource_string_0[] = { 
"attribute vec4 a_position;\n"
"attribute vec2 a_texCoord;\n"
"varying vec2 v_texCoord;\n"
"void main()\n"
"{\n"
"    gl_Position = a_position;\n"
"    v_texCoord = a_texCoord;\n"
"}",
};
const char *const glShaderSource_string_1[] = { 
"#extension GL_OES_EGL_image_external : require\n"
"precision mediump float;\n"
"varying vec2 v_texCoord;\n"
"uniform samplerExternalOES s_texture;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D(s_texture, v_texCoord);\n"
"}",
};

// Private Functions

void SetupReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    CreateEGLImageKHR(gEGLDisplay, gContextMap2[5], 12465, 0, 0, 2, 2, 1);
    glGenTextures(1, (GLuint *)gReadBuffer);
    UpdateTextureID(1, 0);
    glBindTexture(GL_TEXTURE_EXTERNAL_OES, gTextureMap[1]);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MIN_FILTER, 9728);
    glTexParameteri(GL_TEXTURE_EXTERNAL_OES, GL_TEXTURE_MAG_FILTER, 9728);
    UpdateEGLImageData(1, 2, 2, (const GLubyte *)GetBinaryData(352));
    glEGLImageTargetTexture2DOES(GL_TEXTURE_EXTERNAL_OES, gEGLImageMap2[1]);
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
    glBindAttribLocation(gShaderProgramMap[1], 1, "a_texCoord");
    glLinkProgram(gShaderProgramMap[1]);
    UpdateUniformLocation(1, "s_texture", 0, 1);
    glUseProgram(gShaderProgramMap[1]);
    UpdateCurrentProgramPerContext(1);
    glUniform1iv(gUniformLocations[gCurrentProgramPerContext[gCurrentContext]][0], 1, (const GLint *)GetBinaryData(368));
    glDeleteShader(gShaderProgramMap[4]);
    glDeleteShader(gShaderProgramMap[5]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void SetupReplayContextSharedInactive(void)
{
}

// Public Functions

