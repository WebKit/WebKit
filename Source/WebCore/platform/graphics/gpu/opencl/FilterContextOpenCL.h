/*
 * Copyright (C) 2012 University of Szeged
 * Copyright (C) 2012 Tamas Czene <tczene@inf.u-szeged.hu>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY UNIVERSITY OF SZEGED ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL UNIVERSITY OF SZEGED OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef FilterContextOpenCL_h
#define FilterContextOpenCL_h

#if ENABLE(OPENCL)
#include "CL/cl.h"
#include "Color.h"
#include "ColorSpace.h"
#include "IntRect.h"
#include "IntSize.h"
#include "OpenCLHandle.h"

#define PROGRAM_STR(...)  #__VA_ARGS__
#define PROGRAM(...) PROGRAM_STR(__VA_ARGS__)

namespace WebCore {

class FilterContextOpenCL {
public:
    FilterContextOpenCL()
        : m_deviceId(0)
        , m_deviceContext(0)
        , m_commandQueue(0)
        , m_transformColorSpaceProgram(0)
        , m_transformColorSpaceKernel(0)
        , m_colorMatrixCompileStatus(openclNotCompiledYet)
        , m_colorMatrixProgram(0)
        , m_matrixOperation(0)
        , m_saturateAndHueRotateOperation(0)
        , m_luminanceOperation(0)
        , m_turbulenceCompileStatus(openclNotCompiledYet)
        , m_turbulenceProgram(0)
        , m_turbulenceOperation(0)
    {
    }

    // Returns 0 if initialization failed.
    static FilterContextOpenCL* context();

    cl_device_id deviceId() { return m_deviceId; }
    cl_context deviceContext() { return m_deviceContext; }
    cl_command_queue commandQueue() { return m_commandQueue; }

    OpenCLHandle createOpenCLImage(IntSize);
    void openCLTransformColorSpace(OpenCLHandle&, IntRect, ColorSpace, ColorSpace);

    inline bool compileFEColorMatrix();
    inline bool compileFETurbulence();

    inline void applyFEColorMatrix(OpenCLHandle, IntSize, OpenCLHandle, IntPoint, float*, int);
    inline void applyFETurbulence(OpenCLHandle, IntSize, int, void*, void*, void*, void*, void*,
        int*, int, int, int, int, float, float, bool, int, int);

private:

    class RunKernel {
        public:
            RunKernel(FilterContextOpenCL* context, cl_kernel kernel, size_t width, size_t height)
                : m_context(context)
                , m_kernel(kernel)
                , index(0)
            {
                m_globalSize[0] = width;
                m_globalSize[1] = height;
            }

            void addArgument(OpenCLHandle handle)
            {
                clSetKernelArg(m_kernel, index++, sizeof(OpenCLHandle), handle.handleAddress());
            }

            void addArgument(cl_int value)
            {
                clSetKernelArg(m_kernel, index++, sizeof(cl_int), reinterpret_cast<void*>(&value));
            }

            void addArgument(cl_float value)
            {
                clSetKernelArg(m_kernel, index++, sizeof(cl_float), reinterpret_cast<void*>(&value));
            }

            void addArgument(cl_sampler handle)
            {
                clSetKernelArg(m_kernel, index++, sizeof(cl_sampler), reinterpret_cast<void*>(&handle));
            }

            OpenCLHandle addArgument(void* buffer, int size)
            {
                OpenCLHandle handle(clCreateBuffer(m_context->deviceContext(), CL_MEM_READ_ONLY, size, 0, 0));
                clEnqueueWriteBuffer(m_context->commandQueue(), handle, CL_TRUE, 0, size, buffer, 0, 0, 0);
                clSetKernelArg(m_kernel, index++, sizeof(OpenCLHandle), handle.handleAddress());
                return handle;
            }

            void run()
            {
                clFinish(m_context->m_commandQueue);
                clEnqueueNDRangeKernel(m_context->m_commandQueue, m_kernel, 2, 0, m_globalSize, 0, 0, 0, 0);
            }

            FilterContextOpenCL* m_context;
            cl_kernel m_kernel;
            size_t m_globalSize[2];
            int index;
        };

    enum OpenCLCompileStatus {
        openclNotCompiledYet,
        openclCompileFailed,
        openclCompileSuccessful
    };

    static cl_program compileProgram(const char*);
    static inline cl_kernel kernelByName(cl_program program, const char* name) { return clCreateKernel(program, name, 0); }

    static FilterContextOpenCL* m_context;
    static int m_alreadyInitialized;

    cl_device_id m_deviceId;
    cl_context m_deviceContext;
    cl_command_queue m_commandQueue;

    cl_program m_transformColorSpaceProgram;
    cl_kernel m_transformColorSpaceKernel;

    OpenCLCompileStatus m_colorMatrixCompileStatus;
    cl_program m_colorMatrixProgram;
    cl_kernel m_matrixOperation;
    cl_kernel m_saturateAndHueRotateOperation; 
    cl_kernel m_luminanceOperation;

    OpenCLCompileStatus m_turbulenceCompileStatus;
    cl_program m_turbulenceProgram;
    cl_kernel m_turbulenceOperation;
};

} // namespace WebCore

#endif // ENABLE(OPENCL)

#endif
