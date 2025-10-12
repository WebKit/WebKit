//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#include "test_utils/ANGLETestCL.h"

#include <angle_cl.h>
#include "util/test_utils.h"

using namespace angle;

namespace
{

void CL_CALLBACK ContextCreated(const char *errinfo,
                                const void *private_info,
                                size_t cb,
                                void *user_data)
{
    std::cout << "Context created";
}

class CapturedTestCL : public ANGLETestCL<>
{
  protected:
    CapturedTestCL() : ANGLETestCL(this->GetParam()) {}
};

class MultiFrameCL
{
  public:
    MultiFrameCL() {}

    void testSetUp()
    {
        // Get platform
        cl_platform_id platform;
        clGetPlatformIDs(1, &platform, nullptr);

        // Get device
        cl_device_id device;
        clGetDeviceIDs(platform, CL_DEVICE_TYPE_GPU, 1, &device, nullptr);

        // Create context
        context = clCreateContext(nullptr, 1, &device, ContextCreated, nullptr, nullptr);
        assert(context != nullptr);

        // Create command queue
        queue = clCreateCommandQueue(context, device, 0, nullptr);
        assert(queue != nullptr);

        // Create and build program
        const char *kernelSource = R"(
        __kernel void frame1(__global float *output)
        {
            int gid = get_global_id(0);
            output[gid] = gid * 2.0f;
        }

        __kernel void frame2(__global float *output)
        {
            int gid = get_global_id(0);
            output[gid] = gid * gid;
        }

        __kernel void frame3(__global float *output)
        {
            int gid = get_global_id(0);
            output[gid] = gid + 100.0f;
        }

        __kernel void frame4(__global float *output)
        {
            int gid = get_global_id(0);
            output[gid] = gid;
        }

        __kernel void frame5(__global float *output)
        {
            int gid = get_global_id(0);
            output[gid] = gid/gid;
        }
        )";
        program = clCreateProgramWithSource(context, 1, &kernelSource, nullptr, nullptr);
        clBuildProgram(program, 0, nullptr, nullptr, nullptr, nullptr);

        cl_build_status status;
        clGetProgramBuildInfo(program, device, CL_PROGRAM_BUILD_STATUS, sizeof(cl_build_status),
                              &status, nullptr);
        std::cout << "Build status: " << status << "\n";
    }

    void testTearDown()
    {
        clReleaseProgram(program);
        clReleaseCommandQueue(queue);
        clReleaseContext(context);
    }

    void frame1() { executeKernel("frame1"); }

    void frame2() { executeKernel("frame2"); }

    void frame3() { executeKernel("frame3"); }

    void frame4() { executeKernel("frame4"); }

    void frame5() { executeKernel("frame5"); }

  private:
    void executeKernel(const char *kernelName)
    {

        // Create buffer
        outputBuffer =
            clCreateBuffer(context, CL_MEM_READ_WRITE, sizeof(float) * 128, nullptr, nullptr);

        // Get CL_MEM_SIZE
        size_t memSize;
        clGetMemObjectInfo(outputBuffer, CL_MEM_SIZE, sizeof(size_t), &memSize, nullptr);
        std::cout << "Buffer size: " << memSize << ":\n";

        // Create kernel
        cl_kernel kernel = clCreateKernel(program, kernelName, nullptr);
        assert(kernel != nullptr);

        // Set kernel arguments
        clSetKernelArg(kernel, 0, sizeof(cl_mem), &outputBuffer);

        // Execute kernel
        size_t globalWorkSize = 128;
        clEnqueueNDRangeKernel(queue, kernel, 1, nullptr, &globalWorkSize, nullptr, 0, nullptr,
                               nullptr);

        // Read and verify results
        std::vector<float> results(128);
        clEnqueueReadBuffer(queue, outputBuffer, CL_TRUE, 0, sizeof(float) * 128, results.data(), 0,
                            nullptr, nullptr);

        // Print the results
        std::cout << "Results from " << kernelName << ":\n";
        for (size_t i = 0; i < results.size(); ++i)
        {
            std::cout << results[i] << " ";
        }
        std::cout << "\n";

        // Cleanup kernel
        clReleaseKernel(kernel);

        // Cleanup buffer
        clReleaseMemObject(outputBuffer);
    }

    cl_context context;
    cl_command_queue queue;
    cl_program program;
    cl_mem outputBuffer;
};

// OpenCL test captured by capture_tests.py
TEST_P(CapturedTestCL, MultiFrameCL)
{
    MultiFrameCL test;
    test.testSetUp();

    // Execute multiple frames
    test.frame1();
    test.frame2();
    test.frame3();
    test.frame4();
    test.frame5();

    test.testTearDown();
}

GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(CapturedTestCL);
// Capture is only supported on the Vulkan backend
ANGLE_INSTANTIATE_TEST(CapturedTestCL, ES3_VULKAN());
}  // anonymous namespace