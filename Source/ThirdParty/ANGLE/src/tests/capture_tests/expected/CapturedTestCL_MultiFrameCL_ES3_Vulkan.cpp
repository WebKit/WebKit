#include "CapturedTestCL_MultiFrameCL_ES3_Vulkan.h"
#include "trace_fixture_cl.h"

const char clGetExtensionFunctionAddress_func_name_0[] = { "clIcdGetPlatformIDsKHR" };
const char * clCreateProgramWithSource_strings_0[] = { 
"\n"
"        __kernel void frame1(__global float *output)\n"
"        {\n"
"            int gid = get_global_id(0);\n"
"            output[gid] = gid * 2.0f;\n"
"        }\n"
"\n"
"        __kernel void frame2(__global float *output)\n"
"        {\n"
"            int gid = get_global_id(0);\n"
"            output[gid] = gid * gid;\n"
"        }\n"
"\n"
"        __kernel void frame3(__global float *output)\n"
"        {\n"
"            int gid = get_global_id(0);\n"
"            output[gid] = gid + 100.0f;\n"
"        }\n"
"\n"
"        __kernel void frame4(__global float *output)\n"
"        {\n"
"            int gid = get_global_id(0);\n"
"            output[gid] = gid;\n"
"            if (gid == 0)\n"
"            {\n"
"                printf(\"Frame 4!\\n\");\n"
"            }\n"
"        }\n"
"\n"
"        __kernel void frame5(__global float *output)\n"
"        {\n"
"            int gid = get_global_id(0);\n"
"            output[gid] = gid/gid;\n"
"        }\n"
"        ",
};
const char clCreateKernel_kernel_name_4[] = { "frame1" };

// Private Functions

void InitReplay(void)
{
    // binaryDataFileName = CapturedTestCL_MultiFrameCL_ES3_Vulkan.angledata
    // maxClientArraySize = 0
    // readBufferSize = 512
    // clPlatformsMapSize = 8
    // clDevicesMapSize = 8
    // clContextsMapSize = 8
    // clCommandQueuesMapSize = 8
    // clMemMapSize = 8
    // clEventsMapSize = 0
    // clProgramsMapSize = 8
    // clKernelsMapSize = 16
    // clSamplerMapSize = 0
    // clVoidMapSize = 0
    InitializeReplayCL2("CapturedTestCL_MultiFrameCL_ES3_Vulkan.angledata", 0, 512, 8, 8, 8, 8, 8, 0, 8, 16, 0, 0);
    InitializeBinaryDataLoader();
}

// Public Functions

void ReplayFrame(uint32_t frameIndex)
{
    switch (frameIndex)
    {
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

void SetupFirstFrame()
{
    clIcdGetPlatformIDsKHR = (clIcdGetPlatformIDsKHR_fn)clGetExtensionFunctionAddress(clGetExtensionFunctionAddress_func_name_0);
    clGetPlatformIDs(1, clPlatformsMap, NULL);
    temporaryDevicesList.clear();
    temporaryDevicesList.resize(1);
    clGetDeviceIDs(clPlatformsMap[0], 4, 1, temporaryDevicesList.data(), NULL);
    clDevicesMap[0] = temporaryDevicesList[0];
    temporaryDevicesList = {clDevicesMap[0]};
    clContextsMap[0] = clCreateContext(NULL, 1, temporaryDevicesList.data(), NULL, 0, NULL);
    clCommandQueuesMap[0] = clCreateCommandQueue(clContextsMap[0], clDevicesMap[0], 0, NULL);
    clProgramsMap[0] = clCreateProgramWithSource(clContextsMap[0], 1, clCreateProgramWithSource_strings_0, NULL, NULL);
    clBuildProgram(clProgramsMap[0], 0, NULL, 0, NULL, 0);
    clMemMap[0] = clCreateBuffer(clContextsMap[0], 1, 512, 0, NULL);
    clKernelsMap[0] = clCreateKernel(clProgramsMap[0], clCreateKernel_kernel_name_4, NULL);
    clSetKernelArg(clKernelsMap[0], 0, 8, (const void *)&clMemMap[0]);
    clEnqueueWriteBuffer(clCommandQueuesMap[0], clMemMap[0], 1, 0, 512, (const GLubyte *)GetBinaryData(64), 0, NULL, NULL);
}

void ResetReplay(void)
{
    clReleaseContext(clContextsMap[0]);
    clReleaseCommandQueue(clCommandQueuesMap[0]);
    clReleaseProgram(clProgramsMap[0]);
    clReleaseMemObject(clMemMap[0]);
    clReleaseKernel(clKernelsMap[1]);
}

