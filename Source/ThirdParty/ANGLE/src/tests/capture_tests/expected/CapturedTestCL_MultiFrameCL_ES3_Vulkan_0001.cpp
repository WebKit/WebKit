#include "CapturedTestCL_MultiFrameCL_ES3_Vulkan.h"
#include "trace_fixture_cl.h"

const char clCreateKernel_kernel_name_0[] = { "frame2" };

const char clCreateKernel_kernel_name_1[] = { "frame3" };

const char clCreateKernel_kernel_name_2[] = { "frame4" };

const char clCreateKernel_kernel_name_3[] = { "frame5" };

// Private Functions

void ReplayFrame2(void)
{
    clEnqueueReadBuffer(clCommandQueuesMap[0], clMemMap[0], 1, 0, 512, (void *)gReadBuffer, 0, NULL, NULL);
    clReleaseKernel(clKernelsMap[0]);
    clReleaseMemObject(clMemMap[0]);
    clMemMap[0] = clCreateBuffer(clContextsMap[0], 1, 512, 0, NULL);
    clKernelsMap[1] = clCreateKernel(clProgramsMap[0], clCreateKernel_kernel_name_0, NULL);
    clSetKernelArg(clKernelsMap[1], 0, 8, (const void *)&clMemMap[0]);
    clEnqueueNDRangeKernel(clCommandQueuesMap[0], clKernelsMap[1], 1, NULL, (const size_t *)GetBinaryData(0), NULL, 0, NULL, NULL);
}

void ReplayFrame3(void)
{
    clEnqueueReadBuffer(clCommandQueuesMap[0], clMemMap[0], 1, 0, 512, (void *)gReadBuffer, 0, NULL, NULL);
    clReleaseKernel(clKernelsMap[1]);
    clReleaseMemObject(clMemMap[0]);
    clMemMap[0] = clCreateBuffer(clContextsMap[0], 1, 512, 0, NULL);
    clKernelsMap[1] = clCreateKernel(clProgramsMap[0], clCreateKernel_kernel_name_1, NULL);
    clSetKernelArg(clKernelsMap[1], 0, 8, (const void *)&clMemMap[0]);
    clEnqueueNDRangeKernel(clCommandQueuesMap[0], clKernelsMap[1], 1, NULL, (const size_t *)GetBinaryData(16), NULL, 0, NULL, NULL);
}

void ReplayFrame4(void)
{
    clEnqueueReadBuffer(clCommandQueuesMap[0], clMemMap[0], 1, 0, 512, (void *)gReadBuffer, 0, NULL, NULL);
    clReleaseKernel(clKernelsMap[1]);
    clReleaseMemObject(clMemMap[0]);
    clMemMap[0] = clCreateBuffer(clContextsMap[0], 1, 512, 0, NULL);
    clKernelsMap[1] = clCreateKernel(clProgramsMap[0], clCreateKernel_kernel_name_2, NULL);
    clSetKernelArg(clKernelsMap[1], 0, 8, (const void *)&clMemMap[0]);
    clEnqueueNDRangeKernel(clCommandQueuesMap[0], clKernelsMap[1], 1, NULL, (const size_t *)GetBinaryData(32), NULL, 0, NULL, NULL);
}

void ReplayFrame5(void)
{
    clEnqueueReadBuffer(clCommandQueuesMap[0], clMemMap[0], 1, 0, 512, (void *)gReadBuffer, 0, NULL, NULL);
    clReleaseKernel(clKernelsMap[1]);
    clReleaseMemObject(clMemMap[0]);
    clMemMap[0] = clCreateBuffer(clContextsMap[0], 1, 512, 0, NULL);
    clKernelsMap[1] = clCreateKernel(clProgramsMap[0], clCreateKernel_kernel_name_3, NULL);
    clSetKernelArg(clKernelsMap[1], 0, 8, (const void *)&clMemMap[0]);
    clEnqueueNDRangeKernel(clCommandQueuesMap[0], clKernelsMap[1], 1, NULL, (const size_t *)GetBinaryData(48), NULL, 0, NULL, NULL);
}

// Public Functions

void SetupReplay(void)
{
    InitReplay();
}

