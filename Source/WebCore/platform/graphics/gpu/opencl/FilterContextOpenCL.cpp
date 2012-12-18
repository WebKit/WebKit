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

#include "config.h"

#if ENABLE(OPENCL)
#include "FilterContextOpenCL.h"

namespace WebCore {

FilterContextOpenCL* FilterContextOpenCL::m_context = 0;
int FilterContextOpenCL::m_alreadyInitialized = 0;

FilterContextOpenCL* FilterContextOpenCL::context()
{
    if (m_context)
        return m_context;
    if (m_alreadyInitialized)
        return 0;

    m_alreadyInitialized = true;
    FilterContextOpenCL* localContext = new FilterContextOpenCL();

    // Initializing the context.
    cl_int errorNumber;
    cl_device_id* devices;
    cl_platform_id firstPlatformId;
    size_t deviceBufferSize = 0;

    errorNumber = clGetPlatformIDs(1, &firstPlatformId, 0);
    cl_context_properties contextProperties[] = { CL_CONTEXT_PLATFORM, (cl_context_properties)firstPlatformId, 0};
    localContext->m_deviceContext = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_GPU, 0, 0, &errorNumber);
    if (errorNumber != CL_SUCCESS) {
        localContext->m_deviceContext = clCreateContextFromType(contextProperties, CL_DEVICE_TYPE_CPU, 0, 0, &errorNumber);
        if (errorNumber != CL_SUCCESS)
            return 0;
    }

    errorNumber = clGetContextInfo(localContext->m_deviceContext, CL_CONTEXT_DEVICES, 0, 0, &deviceBufferSize);
    if (errorNumber != CL_SUCCESS)
        return 0;

    if (!deviceBufferSize)
        return 0;

    devices = reinterpret_cast<cl_device_id*>(fastMalloc(deviceBufferSize));
    errorNumber = clGetContextInfo(localContext->m_deviceContext, CL_CONTEXT_DEVICES, deviceBufferSize, devices, 0);
    if (errorNumber != CL_SUCCESS)
        return 0;

    localContext->m_commandQueue = clCreateCommandQueue(localContext->m_deviceContext, devices[0], 0, 0);
    if (!localContext->m_commandQueue)
        return 0;

    localContext->m_deviceId = devices[0];
    fastFree(devices);

    cl_bool imageSupport = CL_FALSE;
    clGetDeviceInfo(localContext->m_deviceId, CL_DEVICE_IMAGE_SUPPORT, sizeof(cl_bool), &imageSupport, 0);
    if (imageSupport != CL_TRUE)
        return 0;

    m_context = localContext;
    return m_context;
}

OpenCLHandle FilterContextOpenCL::createOpenCLImage(IntSize paintSize)
{
    FilterContextOpenCL* context = FilterContextOpenCL::context();

    cl_image_format clImageFormat;
    clImageFormat.image_channel_order = CL_RGBA;
    clImageFormat.image_channel_data_type = CL_UNORM_INT8;

    OpenCLHandle image = clCreateImage2D(context->deviceContext(), CL_MEM_READ_WRITE, &clImageFormat,
        paintSize.width(), paintSize.height(), 0, 0, 0);
    return image;
}

static const char* transformColorSpaceKernelProgram =
PROGRAM(
const sampler_t sampler = CLK_NORMALIZED_COORDS_FALSE | CLK_ADDRESS_CLAMP_TO_EDGE | CLK_FILTER_NEAREST;

__kernel void transformColorSpace(__read_only image2d_t source, __write_only image2d_t destination, __constant float *clLookUpTable)
{
    int2 sourceCoord = (int2) (get_global_id(0), get_global_id(1));
    float4 pixel = read_imagef(source, sampler, sourceCoord);

    pixel = (float4) (clLookUpTable[(int)(round(pixel.x * 255))], clLookUpTable[(int)(round(pixel.y * 255))],
        clLookUpTable[(int) (round(pixel.z * 255))], pixel.w);

    write_imagef(destination, sourceCoord, pixel);
}
); // End of OpenCL kernels

void FilterContextOpenCL::openCLTransformColorSpace(OpenCLHandle& source, IntRect sourceSize, ColorSpace srcColorSpace, ColorSpace dstColorSpace)
{
    DEFINE_STATIC_LOCAL(OpenCLHandle, deviceRgbLUT, ());
    DEFINE_STATIC_LOCAL(OpenCLHandle, linearRgbLUT, ());

    if (srcColorSpace == dstColorSpace)
        return;

    if ((srcColorSpace != ColorSpaceLinearRGB && srcColorSpace != ColorSpaceDeviceRGB)
        || (dstColorSpace != ColorSpaceLinearRGB && dstColorSpace != ColorSpaceDeviceRGB))
        return;

    FilterContextOpenCL* context = FilterContextOpenCL::context();
    ASSERT(context);

    OpenCLHandle destination = context->createOpenCLImage(sourceSize.size());

    if (!m_transformColorSpaceProgram) {
        m_transformColorSpaceProgram = compileProgram(transformColorSpaceKernelProgram);
        ASSERT(m_transformColorSpaceProgram);
        m_transformColorSpaceKernel = kernelByName(m_transformColorSpaceProgram, "transformColorSpace");
        ASSERT(m_transformColorSpaceKernel);
    }

    RunKernel kernel(context, m_transformColorSpaceKernel, sourceSize.width(), sourceSize.height());
    kernel.addArgument(source);
    kernel.addArgument(destination);

    if (dstColorSpace == ColorSpaceLinearRGB) {
        if (!linearRgbLUT) {
            Vector<float> lookUpTable;
            for (unsigned i = 0; i < 256; i++) {
                float color = i  / 255.0f;
                color = (color <= 0.04045f ? color / 12.92f : pow((color + 0.055f) / 1.055f, 2.4f));
                color = std::max(0.0f, color);
                color = std::min(1.0f, color);
                lookUpTable.append((round(color * 255)) / 255);
            }
            linearRgbLUT = kernel.addArgument(lookUpTable.data(), sizeof(float) * 256);
        } else
            kernel.addArgument(linearRgbLUT);
    } else if (dstColorSpace == ColorSpaceDeviceRGB) {
        if (!deviceRgbLUT) {
            Vector<float> lookUpTable;
            for (unsigned i = 0; i < 256; i++) {
                float color = i / 255.0f;
                color = (powf(color, 1.0f / 2.4f) * 1.055f) - 0.055f;
                color = std::max(0.0f, color);
                color = std::min(1.0f, color);
                lookUpTable.append((round(color * 255)) / 255);
            }
            deviceRgbLUT = kernel.addArgument(lookUpTable.data(), sizeof(float) * 256);
        } else
            kernel.addArgument(deviceRgbLUT);
    }

    kernel.run();
    source.clear();
    source = destination;
}

cl_program FilterContextOpenCL::compileProgram(const char* source)
{
    cl_program program;
    cl_int errorNumber;

    FilterContextOpenCL* context = FilterContextOpenCL::context();
    ASSERT(context);

    program = clCreateProgramWithSource(context->m_deviceContext, 1, (const char**) &source, 0, 0);
    errorNumber = clBuildProgram(program, 0, 0, 0, 0, 0);
    if (errorNumber)
        return 0;

    return program;
}
} // namespace WebCore

#endif
