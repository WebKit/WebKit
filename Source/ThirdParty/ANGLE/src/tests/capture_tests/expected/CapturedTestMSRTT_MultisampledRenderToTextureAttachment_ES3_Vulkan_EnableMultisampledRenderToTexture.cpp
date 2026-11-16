#include "CapturedTestMSRTT_MultisampledRenderToTextureAttachment_ES3_Vulkan_EnableMultisampledRenderToTexture.h"
#include "trace_fixture.h"
#include "angle_trace_gl.h"

// Private Functions

void InitReplay(void)
{
    // binaryDataFileName = CapturedTestMSRTT_MultisampledRenderToTextureAttachment_ES3_Vulkan_EnableMultisampledRenderToTexture.angledata
    // maxClientArraySize = 0
    // readBufferSize = 8
    // resourceIDBufferSize = 0
    // contextID = 18
    // maxBuffer = 0
    // maxContext = 18
    // maxFenceNV = 0
    // maxFramebuffer = 3
    // maxImage = 0
    // maxMemoryObject = 0
    // maxProgramPipeline = 0
    // maxQuery = 0
    // maxRenderbuffer = 1
    // maxSampler = 0
    // maxSemaphore = 0
    // maxShaderProgram = 0
    // maxSurface = 1
    // maxSync = 0
    // maxTexture = 1
    // maxTransformFeedback = 0
    // maxVertexArray = 0
    // maxegl_Sync = 0
    InitializeReplay5("CapturedTestMSRTT_MultisampledRenderToTextureAttachment_ES3_Vulkan_EnableMultisampledRenderToTexture.angledata", 0, 8, 0, 18, 0, 18, 0, 3, 0, 0, 0, 0, 1, 0, 0, 0, 1, 0, 1, 0, 0, 0);
    InitializeBinaryDataLoader();
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
        case 5:
            ReplayFrame5();
            break;
        default:
            break;
    }
}

