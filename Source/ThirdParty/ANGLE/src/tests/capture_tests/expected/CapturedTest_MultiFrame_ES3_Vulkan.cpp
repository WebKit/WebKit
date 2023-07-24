#include "CapturedTest_MultiFrame_ES3_Vulkan.h"
#include "trace_fixture.h"
#include "angle_trace_gl.h"

// Private Functions

void InitReplay(void)
{
    // binaryDataFileName = CapturedTest_MultiFrame_ES3_Vulkan.angledata
    // maxClientArraySize = 0
    // maxClientArraySize = 0
    // readBufferSize = 32
    // resourceIDBufferSize = 1
    // contextID = 1
    // maxBuffer = 1
    // maxContext = 1
    // maxFenceNV = 0
    // maxFramebuffer = 2
    // maxImage = 0
    // maxMemoryObject = 0
    // maxProgramPipeline = 0
    // maxQuery = 0
    // maxRenderbuffer = 0
    // maxSampler = 0
    // maxSemaphore = 0
    // maxShaderProgram = 3
    // maxSurface = 1
    // maxSync = 0
    // maxTexture = 0
    // maxTransformFeedback = 0
    // maxVertexArray = 0
    // maxegl_Sync = 0
    InitializeReplay4("CapturedTest_MultiFrame_ES3_Vulkan.angledata", 0, 32, 1, 1, 1, 1, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3, 1, 0, 0, 0, 0, 0);
}

// Public Functions

void ReplayFrame(uint32_t frameIndex)
{
    switch (frameIndex)
    {
        case 1:
            ReplayFrame1();
            break;
        case 2:
            ReplayFrame2();
            break;
        case 3:
            ReplayFrame3();
            break;
        case 4:
            ReplayFrame4();
            break;
        default:
            break;
    }
}

