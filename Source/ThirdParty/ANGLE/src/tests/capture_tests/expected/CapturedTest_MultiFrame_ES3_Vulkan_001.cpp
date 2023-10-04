#include "CapturedTest_MultiFrame_ES3_Vulkan.h"
#include "angle_trace_gl.h"

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
"precision mediump float;\n"
"varying vec2 v_texCoord;\n"
"uniform sampler2D s_texture;\n"
"void main()\n"
"{\n"
"    gl_FragColor = texture2D(s_texture, v_texCoord);\n"
"}",
};

const char *const glShaderSource_string_2[] = { 
"precision highp float;\n"
"attribute vec3 attr1;\n"
"void main(void) {\n"
"   gl_Position = vec4(attr1, 1.0);\n"
"}",
};
const char *const glShaderSource_string_3[] = { 
"precision highp float;\n"
"void main(void) {\n"
"   gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
"}",
};

// Private Functions

void SetupReplayContext1(void)
{
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[0], gSurfaceMap2[0], gContextMap2[1]);
    glUseProgram(gShaderProgramMap[0]);
    UpdateCurrentProgram(0);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
}

void ReplayFrame1(void)
{
    eglGetError();
    glClearColor(0.25, 0.5, 0.5, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gShaderProgramMap[3]);
    UpdateCurrentProgram(3);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, gClientArrays[0]);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, gClientArrays[1]);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glUniform1i(gUniformLocations[gCurrentProgram][0], 0);
    UpdateClientArrayPointer(0, (const GLubyte *)&gBinaryData[0], 72);
    UpdateClientArrayPointer(1, (const GLubyte *)&gBinaryData[80], 68);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLubyte *)&gBinaryData[160]);
    glReadPixels(20, 20, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
    UpdateResourceIDBuffer(0, gVertexArrayMap[0]);
glDeleteVertexArrays(1, gResourceIDBuffer);
    UpdateResourceIDBuffer(0, gVertexArrayMap[1]);
glDeleteVertexArrays(1, gResourceIDBuffer);
}

void ReplayFrame2(void)
{
    eglGetError();
    CreateProgram(6);
    CreateShader(GL_VERTEX_SHADER, 7);
    CreateShader(GL_FRAGMENT_SHADER, 8);
    glShaderSource(gShaderProgramMap[7], 1, glShaderSource_string_0, (const GLint *)&gBinaryData[176]);
    glCompileShader(gShaderProgramMap[7]);
    glAttachShader(gShaderProgramMap[6], gShaderProgramMap[7]);
    glShaderSource(gShaderProgramMap[8], 1, glShaderSource_string_1, (const GLint *)&gBinaryData[192]);
    glCompileShader(gShaderProgramMap[8]);
    glAttachShader(gShaderProgramMap[6], gShaderProgramMap[8]);
    glLinkProgram(gShaderProgramMap[6]);
    UpdateUniformLocation(6, "s_texture", 0, 1);
    glGetAttribLocation(gShaderProgramMap[6], "a_position");
    glGetAttribLocation(gShaderProgramMap[6], "a_texCoord");
    glGetUniformLocation(gShaderProgramMap[6], "s_texture");
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glGenTextures(1, (GLuint *)gReadBuffer);
    UpdateTextureID(3, 0);
    glBindTexture(GL_TEXTURE_2D, gTextureMap[3]);
    glGetError();
    glTexImage2D(GL_TEXTURE_2D, 0, 6407, 2, 2, 0, GL_RGB, GL_UNSIGNED_BYTE, (const GLubyte *)&gBinaryData[208]);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, 9728);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, 9728);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(gShaderProgramMap[6]);
    UpdateCurrentProgram(6);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 20, gClientArrays[0]);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 20, gClientArrays[1]);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glUniform1i(gUniformLocations[gCurrentProgram][0], 0);
    UpdateClientArrayPointer(0, (const GLubyte *)&gBinaryData[224], 72);
    UpdateClientArrayPointer(1, (const GLubyte *)&gBinaryData[304], 68);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_SHORT, (const GLubyte *)&gBinaryData[384]);
    glReadPixels(108, 108, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
    UpdateResourceIDBuffer(0, gTextureMap[3]);
glDeleteTextures(1, gResourceIDBuffer);
    UpdateResourceIDBuffer(0, gVertexArrayMap[0]);
glDeleteVertexArrays(1, gResourceIDBuffer);
    UpdateResourceIDBuffer(0, gVertexArrayMap[1]);
glDeleteVertexArrays(1, gResourceIDBuffer);
    glDeleteProgram(gShaderProgramMap[6]);
    DeleteUniformLocations(gShaderProgramMap[6]);
    glDeleteShader(gShaderProgramMap[7]);
    glDeleteShader(gShaderProgramMap[8]);
}

void ReplayFrame3(void)
{
    eglGetError();
    glGenBuffers(1, (GLuint *)gReadBuffer);
    UpdateBufferID(1, 0);
    glBindBuffer(GL_ARRAY_BUFFER, gBufferMap[1]);
    CreateProgram(9);
    CreateShader(GL_VERTEX_SHADER, 10);
    glShaderSource(gShaderProgramMap[10], 1, glShaderSource_string_2, (const GLint *)&gBinaryData[400]);
    glCompileShader(gShaderProgramMap[10]);
    glGetShaderiv(gShaderProgramMap[10], GL_COMPILE_STATUS, (GLint *)gReadBuffer);
    CreateShader(GL_FRAGMENT_SHADER, 11);
    glShaderSource(gShaderProgramMap[11], 1, glShaderSource_string_3, (const GLint *)&gBinaryData[416]);
    glCompileShader(gShaderProgramMap[11]);
    glGetShaderiv(gShaderProgramMap[11], GL_COMPILE_STATUS, (GLint *)gReadBuffer);
    glAttachShader(gShaderProgramMap[9], gShaderProgramMap[10]);
    glDeleteShader(gShaderProgramMap[10]);
    glAttachShader(gShaderProgramMap[9], gShaderProgramMap[11]);
    glDeleteShader(gShaderProgramMap[11]);
    glLinkProgram(gShaderProgramMap[9]);
    glGetError();
    glGetProgramiv(gShaderProgramMap[9], GL_LINK_STATUS, (GLint *)gReadBuffer);
    glBindAttribLocation(gShaderProgramMap[9], 0, "attr1");
    glLinkProgram(gShaderProgramMap[9]);
    glGetError();
    glGetProgramiv(gShaderProgramMap[9], GL_LINK_STATUS, (GLint *)gReadBuffer);
    glUseProgram(gShaderProgramMap[9]);
    UpdateCurrentProgram(9);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_FALSE, 1, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glGetError();
    glDeleteProgram(gShaderProgramMap[9]);
    DeleteUniformLocations(gShaderProgramMap[9]);
    UpdateResourceIDBuffer(0, gBufferMap[1]);
glDeleteBuffers(1, gResourceIDBuffer);
}

void ResetReplayContextShared(void)
{
    glUseProgram(gShaderProgramMap[3]);
    UpdateCurrentProgram(3);
    glUniform1iv(gUniformLocations[gCurrentProgram][0], 1, (const GLint *)&gBinaryData[432]);
    glUniform1iv(gUniformLocations[gCurrentProgram][0], 1, (const GLint *)&gBinaryData[448]);
}

void ResetReplayContext1(void)
{
}

void ReplayFrame4(void)
{
    eglGetError();
}

// Public Functions

void SetupReplay(void)
{
    InitReplay();
    SetupReplayContextShared();
    if (gReplayResourceMode == angle::ReplayResourceMode::All)
    {
        SetupReplayContextSharedInactive();
    }
    SetCurrentContextID(1);
    SetupReplayContext1();

}

void ResetReplay(void)
{
    ResetReplayContextShared();
    ResetReplayContext1();

    // Reset main context state
    glUseProgram(gShaderProgramMap[0]);
    UpdateCurrentProgram(0);
}

