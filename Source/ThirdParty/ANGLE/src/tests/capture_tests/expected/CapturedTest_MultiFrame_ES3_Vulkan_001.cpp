#include "CapturedTest_MultiFrame_ES3_Vulkan.h"
#include "angle_trace_gl.h"

const char *const glShaderSource_string_0[] = { 
"precision highp float;\n"
"attribute vec3 attr1;\n"
"void main(void) {\n"
"   gl_Position = vec4(attr1, 1.0);\n"
"}",
};
const char *const glShaderSource_string_1[] = { 
"precision highp float;\n"
"void main(void) {\n"
"   gl_FragColor = vec4(0.0, 0.0, 0.0, 1.0);\n"
"}",
};

// Private Functions

void SetupReplayContext1(void)
{
    eglMakeCurrent(gEGLDisplay, gSurfaceMap2[0], gSurfaceMap2[0], gContextMap2[1]);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glBindTransformFeedback(GL_TRANSFORM_FEEDBACK, gTransformFeedbackMap[0]);
    glViewport(0, 0, 128, 128);
    glScissor(0, 0, 128, 128);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void ReplayFrame1(void)
{
    eglGetError();
    glClearColor(0.25, 0.5, 0.5, 0.5);
    glClear(GL_COLOR_BUFFER_BIT);
    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_UNSIGNED_BYTE, (void *)gReadBuffer);
    glGetError();
}

void ReplayFrame2(void)
{
    eglGetError();
    glGenBuffers(1, (GLuint *)gReadBuffer);
    UpdateBufferID(1, 0);
    glBindBuffer(GL_ARRAY_BUFFER, gBufferMap[1]);
    CreateProgram(3);
    CreateShader(GL_VERTEX_SHADER, 4);
    glShaderSource(gShaderProgramMap[4], 1, glShaderSource_string_0, (const GLint *)&gBinaryData[0]);
    glCompileShader(gShaderProgramMap[4]);
    glGetShaderiv(gShaderProgramMap[4], GL_COMPILE_STATUS, (GLint *)gReadBuffer);
    CreateShader(GL_FRAGMENT_SHADER, 5);
    glShaderSource(gShaderProgramMap[5], 1, glShaderSource_string_1, (const GLint *)&gBinaryData[16]);
    glCompileShader(gShaderProgramMap[5]);
    glGetShaderiv(gShaderProgramMap[5], GL_COMPILE_STATUS, (GLint *)gReadBuffer);
    glAttachShader(gShaderProgramMap[3], gShaderProgramMap[4]);
    glDeleteShader(gShaderProgramMap[4]);
    glAttachShader(gShaderProgramMap[3], gShaderProgramMap[5]);
    glDeleteShader(gShaderProgramMap[5]);
    glLinkProgram(gShaderProgramMap[3]);
    glGetError();
    glGetProgramiv(gShaderProgramMap[3], GL_LINK_STATUS, (GLint *)gReadBuffer);
    glBindAttribLocation(gShaderProgramMap[3], 0, "attr1");
    glLinkProgram(gShaderProgramMap[3]);
    glGetError();
    glGetProgramiv(gShaderProgramMap[3], GL_LINK_STATUS, (GLint *)gReadBuffer);
    glUseProgram(gShaderProgramMap[3]);
    UpdateCurrentProgram(3);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_UNSIGNED_BYTE, GL_FALSE, 1, 0);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glGetError();
    glDeleteProgram(gShaderProgramMap[3]);
    DeleteUniformLocations(gShaderProgramMap[3]);
    UpdateResourceIDBuffer(0, gBufferMap[1]);
glDeleteBuffers(1, gResourceIDBuffer);
}

void ReplayFrame3(void)
{
    eglGetError();
}

void ResetReplayContextShared(void)
{
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

