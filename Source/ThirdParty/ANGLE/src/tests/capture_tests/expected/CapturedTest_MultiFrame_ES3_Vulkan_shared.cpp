#include "angle_trace_gl.h"
#include "CapturedTest_MultiFrame_ES3_Vulkan.h"

const char *const glShaderSource_string_2[] = { 
"precision highp float;\n"
"void main(void) {\n"
"   gl_Position = vec4(0.5, 0.5, 0.5, 1.0);\n"
"}",
};

// Private Functions

void SetupReplayContextShared(void)
{
    glPixelStorei(GL_UNPACK_ALIGNMENT, 1);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
}

void SetupReplayContextSharedInactive(void)
{
    CreateShader(GL_VERTEX_SHADER, 2);
    glShaderSource(gShaderProgramMap[2], 1, glShaderSource_string_2, 0);
    glCompileShader(gShaderProgramMap[2]);
}

// Public Functions

