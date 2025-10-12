//
// Copyright 2025 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// capture_cl_params.cpp:
//   Pointer parameter capture functions for the OpenCL entry points.
//

#ifdef UNSAFE_BUFFERS_BUILD
#    pragma allow_unsafe_buffers
#endif

#include "libANGLE/CLImage.h"
#include "libANGLE/capture/capture_cl_autogen.h"
#include "libANGLE/cl_utils.h"
#include "libANGLE/queryutils.h"

namespace cl
{
void CaptureGetPlatformIDs_platforms(bool isCallValid,
                                     cl_uint num_entries,
                                     cl_platform_id *platforms,
                                     cl_uint *num_platforms,
                                     angle::ParamCapture *paramCapture)
{
    if (platforms)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLPlatformIndices(platforms,
                                                                                  num_entries);
    }
}
void CaptureGetPlatformIDs_num_platforms(bool isCallValid,
                                         cl_uint num_entries,
                                         cl_platform_id *platforms,
                                         cl_uint *num_platforms,
                                         angle::ParamCapture *paramCapture)
{
    if (num_platforms)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_uint);
    }
}
void CaptureGetPlatformInfo_param_value(bool isCallValid,
                                        cl_platform_id platform,
                                        PlatformInfo param_namePacked,
                                        size_t param_value_size,
                                        void *param_value,
                                        size_t *param_value_size_ret,
                                        angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetPlatformInfo_param_value_size_ret(bool isCallValid,
                                                 cl_platform_id platform,
                                                 PlatformInfo param_namePacked,
                                                 size_t param_value_size,
                                                 void *param_value,
                                                 size_t *param_value_size_ret,
                                                 angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetDeviceIDs_devices(bool isCallValid,
                                 cl_platform_id platform,
                                 DeviceType device_typePacked,
                                 cl_uint num_entries,
                                 cl_device_id *devices,
                                 cl_uint *num_devices,
                                 angle::ParamCapture *paramCapture)
{
    if (devices)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLDeviceIndices(devices,
                                                                                num_entries);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            devices, num_entries, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureGetDeviceIDs_num_devices(bool isCallValid,
                                     cl_platform_id platform,
                                     DeviceType device_typePacked,
                                     cl_uint num_entries,
                                     cl_device_id *devices,
                                     cl_uint *num_devices,
                                     angle::ParamCapture *paramCapture)
{
    if (num_devices)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_uint);
    }
}
void CaptureGetDeviceInfo_param_value(bool isCallValid,
                                      cl_device_id device,
                                      DeviceInfo param_namePacked,
                                      size_t param_value_size,
                                      void *param_value,
                                      size_t *param_value_size_ret,
                                      angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetDeviceInfo_param_value_size_ret(bool isCallValid,
                                               cl_device_id device,
                                               DeviceInfo param_namePacked,
                                               size_t param_value_size,
                                               void *param_value,
                                               size_t *param_value_size_ret,
                                               angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureCreateContext_properties(bool isCallValid,
                                     const cl_context_properties *properties,
                                     cl_uint num_devices,
                                     const cl_device_id *devices,
                                     void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                   const void *private_info,
                                                                   size_t cb,
                                                                   void *user_data),
                                     void *user_data,
                                     cl_int *errcode_ret,
                                     angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        int propertiesSize = 1;
        while (properties[propertiesSize - 1] != 0)
        {
            ++propertiesSize;
        }
        CaptureMemory(properties, propertiesSize * sizeof(cl_context_properties), paramCapture);
    }
}
void CaptureCreateContext_devices(bool isCallValid,
                                  const cl_context_properties *properties,
                                  cl_uint num_devices,
                                  const cl_device_id *devices,
                                  void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                const void *private_info,
                                                                size_t cb,
                                                                void *user_data),
                                  void *user_data,
                                  cl_int *errcode_ret,
                                  angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        devices, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureCreateContext_pfn_notify(bool isCallValid,
                                     const cl_context_properties *properties,
                                     cl_uint num_devices,
                                     const cl_device_id *devices,
                                     void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                   const void *private_info,
                                                                   size_t cb,
                                                                   void *user_data),
                                     void *user_data,
                                     cl_int *errcode_ret,
                                     angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureCreateContext_user_data(bool isCallValid,
                                    const cl_context_properties *properties,
                                    cl_uint num_devices,
                                    const cl_device_id *devices,
                                    void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                  const void *private_info,
                                                                  size_t cb,
                                                                  void *user_data),
                                    void *user_data,
                                    cl_int *errcode_ret,
                                    angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureCreateContext_errcode_ret(bool isCallValid,
                                      const cl_context_properties *properties,
                                      cl_uint num_devices,
                                      const cl_device_id *devices,
                                      void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                    const void *private_info,
                                                                    size_t cb,
                                                                    void *user_data),
                                      void *user_data,
                                      cl_int *errcode_ret,
                                      angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateContextFromType_properties(bool isCallValid,
                                             const cl_context_properties *properties,
                                             DeviceType device_typePacked,
                                             void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                           const void *private_info,
                                                                           size_t cb,
                                                                           void *user_data),
                                             void *user_data,
                                             cl_int *errcode_ret,
                                             angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        int propertiesSize = 0;
        while (properties[propertiesSize++] != 0)
        {
        }
        CaptureMemory(properties, propertiesSize * sizeof(cl_context_properties), paramCapture);
    }
}
void CaptureCreateContextFromType_pfn_notify(bool isCallValid,
                                             const cl_context_properties *properties,
                                             DeviceType device_typePacked,
                                             void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                           const void *private_info,
                                                                           size_t cb,
                                                                           void *user_data),
                                             void *user_data,
                                             cl_int *errcode_ret,
                                             angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureCreateContextFromType_user_data(bool isCallValid,
                                            const cl_context_properties *properties,
                                            DeviceType device_typePacked,
                                            void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                                                          const void *private_info,
                                                                          size_t cb,
                                                                          void *user_data),
                                            void *user_data,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureCreateContextFromType_errcode_ret(
    bool isCallValid,
    const cl_context_properties *properties,
    DeviceType device_typePacked,
    void(CL_CALLBACK *pfn_notify)(const char *errinfo,
                                  const void *private_info,
                                  size_t cb,
                                  void *user_data),
    void *user_data,
    cl_int *errcode_ret,
    angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureGetContextInfo_param_value(bool isCallValid,
                                       cl_context context,
                                       ContextInfo param_namePacked,
                                       size_t param_value_size,
                                       void *param_value,
                                       size_t *param_value_size_ret,
                                       angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetContextInfo_param_value_size_ret(bool isCallValid,
                                                cl_context context,
                                                ContextInfo param_namePacked,
                                                size_t param_value_size,
                                                void *param_value,
                                                size_t *param_value_size_ret,
                                                angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetCommandQueueInfo_param_value(bool isCallValid,
                                            cl_command_queue command_queue,
                                            CommandQueueInfo param_namePacked,
                                            size_t param_value_size,
                                            void *param_value,
                                            size_t *param_value_size_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetCommandQueueInfo_param_value_size_ret(bool isCallValid,
                                                     cl_command_queue command_queue,
                                                     CommandQueueInfo param_namePacked,
                                                     size_t param_value_size,
                                                     void *param_value,
                                                     size_t *param_value_size_ret,
                                                     angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureCreateBuffer_host_ptr(bool isCallValid,
                                  cl_context context,
                                  MemFlags flagsPacked,
                                  size_t size,
                                  void *host_ptr,
                                  cl_int *errcode_ret,
                                  angle::ParamCapture *paramCapture)
{
    if (host_ptr)
    {
        CaptureMemory(host_ptr, size, paramCapture);
    }
}
void CaptureCreateBuffer_errcode_ret(bool isCallValid,
                                     cl_context context,
                                     MemFlags flagsPacked,
                                     size_t size,
                                     void *host_ptr,
                                     cl_int *errcode_ret,
                                     angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureGetSupportedImageFormats_image_formats(bool isCallValid,
                                                   cl_context context,
                                                   MemFlags flagsPacked,
                                                   MemObjectType image_typePacked,
                                                   cl_uint num_entries,
                                                   cl_image_format *image_formats,
                                                   cl_uint *num_image_formats,
                                                   angle::ParamCapture *paramCapture)
{
    if (image_formats)
    {
        paramCapture->readBufferSizeBytes = num_entries * sizeof(cl_image_format);
    }
}
void CaptureGetSupportedImageFormats_num_image_formats(bool isCallValid,
                                                       cl_context context,
                                                       MemFlags flagsPacked,
                                                       MemObjectType image_typePacked,
                                                       cl_uint num_entries,
                                                       cl_image_format *image_formats,
                                                       cl_uint *num_image_formats,
                                                       angle::ParamCapture *paramCapture)
{
    if (num_image_formats)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_uint);
    }
}
void CaptureGetMemObjectInfo_param_value(bool isCallValid,
                                         cl_mem memobj,
                                         MemInfo param_namePacked,
                                         size_t param_value_size,
                                         void *param_value,
                                         size_t *param_value_size_ret,
                                         angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetMemObjectInfo_param_value_size_ret(bool isCallValid,
                                                  cl_mem memobj,
                                                  MemInfo param_namePacked,
                                                  size_t param_value_size,
                                                  void *param_value,
                                                  size_t *param_value_size_ret,
                                                  angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetImageInfo_param_value(bool isCallValid,
                                     cl_mem image,
                                     ImageInfo param_namePacked,
                                     size_t param_value_size,
                                     void *param_value,
                                     size_t *param_value_size_ret,
                                     angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetImageInfo_param_value_size_ret(bool isCallValid,
                                              cl_mem image,
                                              ImageInfo param_namePacked,
                                              size_t param_value_size,
                                              void *param_value,
                                              size_t *param_value_size_ret,
                                              angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetSamplerInfo_param_value(bool isCallValid,
                                       cl_sampler sampler,
                                       SamplerInfo param_namePacked,
                                       size_t param_value_size,
                                       void *param_value,
                                       size_t *param_value_size_ret,
                                       angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetSamplerInfo_param_value_size_ret(bool isCallValid,
                                                cl_sampler sampler,
                                                SamplerInfo param_namePacked,
                                                size_t param_value_size,
                                                void *param_value,
                                                size_t *param_value_size_ret,
                                                angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureCreateProgramWithSource_strings(bool isCallValid,
                                            cl_context context,
                                            cl_uint count,
                                            const char **strings,
                                            const size_t *lengths,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    for (size_t i = 0; i < count; ++i)
    {
        if (lengths && lengths[i])
        {
            // Add a null terminator so it can be printed in the replay file.
            // It won't effect the function parameters.
            char *tempCharList = new char[lengths[i] + 1];
            std::memcpy(tempCharList, strings[i], lengths[i] * sizeof(char));
            tempCharList[lengths[i] + 1] = '\0';
            CaptureMemory(strings[i], (lengths[i] + 1) * sizeof(char), paramCapture);
            delete[] tempCharList;
        }
        else
        {
            CaptureMemory(strings[i], (strlen(strings[i]) + 1) * sizeof(char), paramCapture);
        }
    }
}
void CaptureCreateProgramWithSource_lengths(bool isCallValid,
                                            cl_context context,
                                            cl_uint count,
                                            const char **strings,
                                            const size_t *lengths,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (lengths)
    {
        CaptureMemory(lengths, count * sizeof(size_t), paramCapture);
    }
}
void CaptureCreateProgramWithSource_errcode_ret(bool isCallValid,
                                                cl_context context,
                                                cl_uint count,
                                                const char **strings,
                                                const size_t *lengths,
                                                cl_int *errcode_ret,
                                                angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateProgramWithBinary_device_list(bool isCallValid,
                                                cl_context context,
                                                cl_uint num_devices,
                                                const cl_device_id *device_list,
                                                const size_t *lengths,
                                                const unsigned char **binaries,
                                                cl_int *binary_status,
                                                cl_int *errcode_ret,
                                                angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        device_list, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureCreateProgramWithBinary_lengths(bool isCallValid,
                                            cl_context context,
                                            cl_uint num_devices,
                                            const cl_device_id *device_list,
                                            const size_t *lengths,
                                            const unsigned char **binaries,
                                            cl_int *binary_status,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (lengths)
    {
        CaptureMemory(lengths, num_devices * sizeof(size_t), paramCapture);
    }
}
void CaptureCreateProgramWithBinary_binaries(bool isCallValid,
                                             cl_context context,
                                             cl_uint num_devices,
                                             const cl_device_id *device_list,
                                             const size_t *lengths,
                                             const unsigned char **binaries,
                                             cl_int *binary_status,
                                             cl_int *errcode_ret,
                                             angle::ParamCapture *paramCapture)
{
    for (size_t i = 0; i < num_devices; ++i)
    {
        if (lengths && lengths[i])
        {
            CaptureMemory(binaries[i], lengths[i] * sizeof(unsigned char), paramCapture);
        }
    }
}

void CaptureCreateProgramWithBinary_binary_status(bool isCallValid,
                                                  cl_context context,
                                                  cl_uint num_devices,
                                                  const cl_device_id *device_list,
                                                  const size_t *lengths,
                                                  const unsigned char **binaries,
                                                  cl_int *binary_status,
                                                  cl_int *errcode_ret,
                                                  angle::ParamCapture *paramCapture)
{
    if (binary_status)
    {
        paramCapture->readBufferSizeBytes = num_devices * sizeof(cl_int);
    }
}

void CaptureCreateProgramWithBinary_errcode_ret(bool isCallValid,
                                                cl_context context,
                                                cl_uint num_devices,
                                                const cl_device_id *device_list,
                                                const size_t *lengths,
                                                const unsigned char **binaries,
                                                cl_int *binary_status,
                                                cl_int *errcode_ret,
                                                angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}

void CaptureBuildProgram_device_list(bool isCallValid,
                                     cl_program program,
                                     cl_uint num_devices,
                                     const cl_device_id *device_list,
                                     const char *options,
                                     void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                   void *user_data),
                                     void *user_data,
                                     angle::ParamCapture *paramCapture)
{
    if (device_list)
    {

        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            device_list, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}

void CaptureBuildProgram_options(bool isCallValid,
                                 cl_program program,
                                 cl_uint num_devices,
                                 const cl_device_id *device_list,
                                 const char *options,
                                 void(CL_CALLBACK *pfn_notify)(cl_program program, void *user_data),
                                 void *user_data,
                                 angle::ParamCapture *paramCapture)
{
    if (options)
    {
        CaptureString(options, paramCapture);
    }
}
void CaptureBuildProgram_pfn_notify(bool isCallValid,
                                    cl_program program,
                                    cl_uint num_devices,
                                    const cl_device_id *device_list,
                                    const char *options,
                                    void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                  void *user_data),
                                    void *user_data,
                                    angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureBuildProgram_user_data(bool isCallValid,
                                   cl_program program,
                                   cl_uint num_devices,
                                   const cl_device_id *device_list,
                                   const char *options,
                                   void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                 void *user_data),
                                   void *user_data,
                                   angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureGetProgramInfo_param_value(bool isCallValid,
                                       cl_program program,
                                       ProgramInfo param_namePacked,
                                       size_t param_value_size,
                                       void *param_value,
                                       size_t *param_value_size_ret,
                                       angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetProgramInfo_param_value_size_ret(bool isCallValid,
                                                cl_program program,
                                                ProgramInfo param_namePacked,
                                                size_t param_value_size,
                                                void *param_value,
                                                size_t *param_value_size_ret,
                                                angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetProgramBuildInfo_param_value(bool isCallValid,
                                            cl_program program,
                                            cl_device_id device,
                                            ProgramBuildInfo param_namePacked,
                                            size_t param_value_size,
                                            void *param_value,
                                            size_t *param_value_size_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetProgramBuildInfo_param_value_size_ret(bool isCallValid,
                                                     cl_program program,
                                                     cl_device_id device,
                                                     ProgramBuildInfo param_namePacked,
                                                     size_t param_value_size,
                                                     void *param_value,
                                                     size_t *param_value_size_ret,
                                                     angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureCreateKernel_kernel_name(bool isCallValid,
                                     cl_program program,
                                     const char *kernel_name,
                                     cl_int *errcode_ret,
                                     angle::ParamCapture *paramCapture)
{
    CaptureString(kernel_name, paramCapture);
}
void CaptureCreateKernel_errcode_ret(bool isCallValid,
                                     cl_program program,
                                     const char *kernel_name,
                                     cl_int *errcode_ret,
                                     angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateKernelsInProgram_kernels(bool isCallValid,
                                           cl_program program,
                                           cl_uint num_kernels,
                                           cl_kernel *kernels,
                                           cl_uint *num_kernels_ret,
                                           angle::ParamCapture *paramCapture)
{
    if (kernels)
    {
        cl_uint maxKernels =
            num_kernels_ret && *num_kernels_ret < num_kernels ? *num_kernels_ret : num_kernels;
        for (cl_uint i = 0; i < maxKernels; ++i)
        {
            cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(&kernels[i]);
        }
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            kernels, maxKernels, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureCreateKernelsInProgram_num_kernels_ret(bool isCallValid,
                                                   cl_program program,
                                                   cl_uint num_kernels,
                                                   cl_kernel *kernels,
                                                   cl_uint *num_kernels_ret,
                                                   angle::ParamCapture *paramCapture)
{
    if (num_kernels_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_uint);
    }
}
void CaptureSetKernelArg_arg_value(bool isCallValid,
                                   cl_kernel kernel,
                                   cl_uint arg_index,
                                   size_t arg_size,
                                   const void *arg_value,
                                   angle::ParamCapture *paramCapture)
{
    if (arg_size == sizeof(cl_mem) && cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                                          static_cast<const cl_mem *>(arg_value)) != SIZE_MAX)
    {
        InitParamValue(angle::ParamType::Tcl_mem, *static_cast<const cl_mem *>(arg_value),
                       &paramCapture->value);
    }
    else if (arg_size == sizeof(cl_sampler) &&
             cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                 static_cast<const cl_sampler *>(arg_value)) != SIZE_MAX)
    {
        InitParamValue(angle::ParamType::Tcl_sampler, *static_cast<const cl_sampler *>(arg_value),
                       &paramCapture->value);
    }
    else if (arg_size == sizeof(cl_command_queue) &&
             cl::Platform::GetDefault()->getFrameCaptureShared()->getIndex(
                 static_cast<const cl_command_queue *>(arg_value)) != SIZE_MAX)
    {
        InitParamValue(angle::ParamType::Tcl_command_queue,
                       *static_cast<const cl_command_queue *>(arg_value), &paramCapture->value);
    }
    else if (arg_value)
    {
        CaptureMemory(arg_value, arg_size, paramCapture);
    }
}
void CaptureGetKernelInfo_param_value(bool isCallValid,
                                      cl_kernel kernel,
                                      KernelInfo param_namePacked,
                                      size_t param_value_size,
                                      void *param_value,
                                      size_t *param_value_size_ret,
                                      angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetKernelInfo_param_value_size_ret(bool isCallValid,
                                               cl_kernel kernel,
                                               KernelInfo param_namePacked,
                                               size_t param_value_size,
                                               void *param_value,
                                               size_t *param_value_size_ret,
                                               angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetKernelWorkGroupInfo_param_value(bool isCallValid,
                                               cl_kernel kernel,
                                               cl_device_id device,
                                               KernelWorkGroupInfo param_namePacked,
                                               size_t param_value_size,
                                               void *param_value,
                                               size_t *param_value_size_ret,
                                               angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetKernelWorkGroupInfo_param_value_size_ret(bool isCallValid,
                                                        cl_kernel kernel,
                                                        cl_device_id device,
                                                        KernelWorkGroupInfo param_namePacked,
                                                        size_t param_value_size,
                                                        void *param_value,
                                                        size_t *param_value_size_ret,
                                                        angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureWaitForEvents_event_list(bool isCallValid,
                                     cl_uint num_events,
                                     const cl_event *event_list,
                                     angle::ParamCapture *paramCapture)
{
    if (event_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_list, num_events, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureGetEventInfo_param_value(bool isCallValid,
                                     cl_event event,
                                     EventInfo param_namePacked,
                                     size_t param_value_size,
                                     void *param_value,
                                     size_t *param_value_size_ret,
                                     angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetEventInfo_param_value_size_ret(bool isCallValid,
                                              cl_event event,
                                              EventInfo param_namePacked,
                                              size_t param_value_size,
                                              void *param_value,
                                              size_t *param_value_size_ret,
                                              angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureGetEventProfilingInfo_param_value(bool isCallValid,
                                              cl_event event,
                                              ProfilingInfo param_namePacked,
                                              size_t param_value_size,
                                              void *param_value,
                                              size_t *param_value_size_ret,
                                              angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetEventProfilingInfo_param_value_size_ret(bool isCallValid,
                                                       cl_event event,
                                                       ProfilingInfo param_namePacked,
                                                       size_t param_value_size,
                                                       void *param_value,
                                                       size_t *param_value_size_ret,
                                                       angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureEnqueueReadBuffer_ptr(bool isCallValid,
                                  cl_command_queue command_queue,
                                  cl_mem buffer,
                                  cl_bool blocking_read,
                                  size_t offset,
                                  size_t size,
                                  void *ptr,
                                  cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list,
                                  cl_event *event,
                                  angle::ParamCapture *paramCapture)
{
    paramCapture->readBufferSizeBytes = size;
}
void CaptureEnqueueReadBuffer_event_wait_list(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_mem buffer,
                                              cl_bool blocking_read,
                                              size_t offset,
                                              size_t size,
                                              void *ptr,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueReadBuffer_event(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem buffer,
                                    cl_bool blocking_read,
                                    size_t offset,
                                    size_t size,
                                    void *ptr,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueWriteBuffer_ptr(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem buffer,
                                   cl_bool blocking_write,
                                   size_t offset,
                                   size_t size,
                                   const void *ptr,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   angle::ParamCapture *paramCapture)
{
    if (ptr)
    {
        CaptureMemory(ptr, size, paramCapture);
    }
}
void CaptureEnqueueWriteBuffer_event_wait_list(bool isCallValid,
                                               cl_command_queue command_queue,
                                               cl_mem buffer,
                                               cl_bool blocking_write,
                                               size_t offset,
                                               size_t size,
                                               const void *ptr,
                                               cl_uint num_events_in_wait_list,
                                               const cl_event *event_wait_list,
                                               cl_event *event,
                                               angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueWriteBuffer_event(bool isCallValid,
                                     cl_command_queue command_queue,
                                     cl_mem buffer,
                                     cl_bool blocking_write,
                                     size_t offset,
                                     size_t size,
                                     const void *ptr,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueCopyBuffer_event_wait_list(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_mem src_buffer,
                                              cl_mem dst_buffer,
                                              size_t src_offset,
                                              size_t dst_offset,
                                              size_t size,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueCopyBuffer_event(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem src_buffer,
                                    cl_mem dst_buffer,
                                    size_t src_offset,
                                    size_t dst_offset,
                                    size_t size,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueReadImage_origin(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem image,
                                    cl_bool blocking_read,
                                    const size_t *origin,
                                    const size_t *region,
                                    size_t row_pitch,
                                    size_t slice_pitch,
                                    void *ptr,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (origin)
    {
        CaptureMemory(origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueReadImage_region(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem image,
                                    cl_bool blocking_read,
                                    const size_t *origin,
                                    const size_t *region,
                                    size_t row_pitch,
                                    size_t slice_pitch,
                                    void *ptr,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueReadImage_ptr(bool isCallValid,
                                 cl_command_queue command_queue,
                                 cl_mem image,
                                 cl_bool blocking_read,
                                 const size_t *origin,
                                 const size_t *region,
                                 size_t row_pitch,
                                 size_t slice_pitch,
                                 void *ptr,
                                 cl_uint num_events_in_wait_list,
                                 const cl_event *event_wait_list,
                                 cl_event *event,
                                 angle::ParamCapture *paramCapture)
{
    if (ptr)
    {
        size_t elementSize        = image->cast<Image>().getElementSize();
        size_t computedRowPitch   = (row_pitch != 0) ? row_pitch : region[0] * elementSize;
        size_t computedSlicePitch = 0;
        MemObjectType imageType   = image->cast<Image>().getType();
        if (imageType == MemObjectType::Image3D || imageType == MemObjectType::Image2D_Array ||
            imageType == MemObjectType::Image1D_Array)
        {
            computedSlicePitch = (slice_pitch != 0) ? slice_pitch : computedRowPitch * region[1];
        }
        paramCapture->readBufferSizeBytes = (region[2] - 1) * computedSlicePitch +
                                            (region[1] - 1) * computedRowPitch +
                                            region[0] * elementSize;
    }
}
void CaptureEnqueueReadImage_event_wait_list(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem image,
                                             cl_bool blocking_read,
                                             const size_t *origin,
                                             const size_t *region,
                                             size_t row_pitch,
                                             size_t slice_pitch,
                                             void *ptr,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueReadImage_event(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem image,
                                   cl_bool blocking_read,
                                   const size_t *origin,
                                   const size_t *region,
                                   size_t row_pitch,
                                   size_t slice_pitch,
                                   void *ptr,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueWriteImage_origin(bool isCallValid,
                                     cl_command_queue command_queue,
                                     cl_mem image,
                                     cl_bool blocking_write,
                                     const size_t *origin,
                                     const size_t *region,
                                     size_t input_row_pitch,
                                     size_t input_slice_pitch,
                                     const void *ptr,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{
    if (origin)
    {
        CaptureMemory(origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueWriteImage_region(bool isCallValid,
                                     cl_command_queue command_queue,
                                     cl_mem image,
                                     cl_bool blocking_write,
                                     const size_t *origin,
                                     const size_t *region,
                                     size_t input_row_pitch,
                                     size_t input_slice_pitch,
                                     const void *ptr,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueWriteImage_ptr(bool isCallValid,
                                  cl_command_queue command_queue,
                                  cl_mem image,
                                  cl_bool blocking_write,
                                  const size_t *origin,
                                  const size_t *region,
                                  size_t input_row_pitch,
                                  size_t input_slice_pitch,
                                  const void *ptr,
                                  cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list,
                                  cl_event *event,
                                  angle::ParamCapture *paramCapture)
{
    if (ptr)
    {
        size_t elementSize = image->cast<Image>().getElementSize();
        size_t computedRowPitch =
            (input_row_pitch != 0) ? input_row_pitch : region[0] * elementSize;
        size_t computedSlicePitch = 0;
        MemObjectType imageType   = image->cast<Image>().getType();
        if (imageType == MemObjectType::Image3D || imageType == MemObjectType::Image2D_Array ||
            imageType == MemObjectType::Image1D_Array)
        {
            computedSlicePitch =
                (input_slice_pitch != 0) ? input_slice_pitch : computedRowPitch * region[1];
        }
        size_t totalSize = (region[2] - 1) * computedSlicePitch +
                           (region[1] - 1) * computedRowPitch + region[0] * elementSize;
        CaptureMemory(ptr, totalSize, paramCapture);
    }
}
void CaptureEnqueueWriteImage_event_wait_list(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_mem image,
                                              cl_bool blocking_write,
                                              const size_t *origin,
                                              const size_t *region,
                                              size_t input_row_pitch,
                                              size_t input_slice_pitch,
                                              const void *ptr,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueWriteImage_event(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem image,
                                    cl_bool blocking_write,
                                    const size_t *origin,
                                    const size_t *region,
                                    size_t input_row_pitch,
                                    size_t input_slice_pitch,
                                    const void *ptr,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueCopyImage_src_origin(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem src_image,
                                        cl_mem dst_image,
                                        const size_t *src_origin,
                                        const size_t *dst_origin,
                                        const size_t *region,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (src_origin)
    {
        CaptureMemory(src_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyImage_dst_origin(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem src_image,
                                        cl_mem dst_image,
                                        const size_t *src_origin,
                                        const size_t *dst_origin,
                                        const size_t *region,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (dst_origin)
    {
        CaptureMemory(dst_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyImage_region(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem src_image,
                                    cl_mem dst_image,
                                    const size_t *src_origin,
                                    const size_t *dst_origin,
                                    const size_t *region,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyImage_event_wait_list(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem src_image,
                                             cl_mem dst_image,
                                             const size_t *src_origin,
                                             const size_t *dst_origin,
                                             const size_t *region,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueCopyImage_event(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem src_image,
                                   cl_mem dst_image,
                                   const size_t *src_origin,
                                   const size_t *dst_origin,
                                   const size_t *region,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueCopyImageToBuffer_src_origin(bool isCallValid,
                                                cl_command_queue command_queue,
                                                cl_mem src_image,
                                                cl_mem dst_buffer,
                                                const size_t *src_origin,
                                                const size_t *region,
                                                size_t dst_offset,
                                                cl_uint num_events_in_wait_list,
                                                const cl_event *event_wait_list,
                                                cl_event *event,
                                                angle::ParamCapture *paramCapture)
{
    if (src_origin)
    {
        CaptureMemory(src_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyImageToBuffer_region(bool isCallValid,
                                            cl_command_queue command_queue,
                                            cl_mem src_image,
                                            cl_mem dst_buffer,
                                            const size_t *src_origin,
                                            const size_t *region,
                                            size_t dst_offset,
                                            cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list,
                                            cl_event *event,
                                            angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyImageToBuffer_event_wait_list(bool isCallValid,
                                                     cl_command_queue command_queue,
                                                     cl_mem src_image,
                                                     cl_mem dst_buffer,
                                                     const size_t *src_origin,
                                                     const size_t *region,
                                                     size_t dst_offset,
                                                     cl_uint num_events_in_wait_list,
                                                     const cl_event *event_wait_list,
                                                     cl_event *event,
                                                     angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueCopyImageToBuffer_event(bool isCallValid,
                                           cl_command_queue command_queue,
                                           cl_mem src_image,
                                           cl_mem dst_buffer,
                                           const size_t *src_origin,
                                           const size_t *region,
                                           size_t dst_offset,
                                           cl_uint num_events_in_wait_list,
                                           const cl_event *event_wait_list,
                                           cl_event *event,
                                           angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueCopyBufferToImage_dst_origin(bool isCallValid,
                                                cl_command_queue command_queue,
                                                cl_mem src_buffer,
                                                cl_mem dst_image,
                                                size_t src_offset,
                                                const size_t *dst_origin,
                                                const size_t *region,
                                                cl_uint num_events_in_wait_list,
                                                const cl_event *event_wait_list,
                                                cl_event *event,
                                                angle::ParamCapture *paramCapture)
{
    if (dst_origin)
    {
        CaptureMemory(dst_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyBufferToImage_region(bool isCallValid,
                                            cl_command_queue command_queue,
                                            cl_mem src_buffer,
                                            cl_mem dst_image,
                                            size_t src_offset,
                                            const size_t *dst_origin,
                                            const size_t *region,
                                            cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list,
                                            cl_event *event,
                                            angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyBufferToImage_event_wait_list(bool isCallValid,
                                                     cl_command_queue command_queue,
                                                     cl_mem src_buffer,
                                                     cl_mem dst_image,
                                                     size_t src_offset,
                                                     const size_t *dst_origin,
                                                     const size_t *region,
                                                     cl_uint num_events_in_wait_list,
                                                     const cl_event *event_wait_list,
                                                     cl_event *event,
                                                     angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueCopyBufferToImage_event(bool isCallValid,
                                           cl_command_queue command_queue,
                                           cl_mem src_buffer,
                                           cl_mem dst_image,
                                           size_t src_offset,
                                           const size_t *dst_origin,
                                           const size_t *region,
                                           cl_uint num_events_in_wait_list,
                                           const cl_event *event_wait_list,
                                           cl_event *event,
                                           angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueMapBuffer_event_wait_list(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem buffer,
                                             cl_bool blocking_map,
                                             MapFlags map_flagsPacked,
                                             size_t offset,
                                             size_t size,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             cl_int *errcode_ret,
                                             angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueMapBuffer_event(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem buffer,
                                   cl_bool blocking_map,
                                   MapFlags map_flagsPacked,
                                   size_t offset,
                                   size_t size,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueMapBuffer_errcode_ret(bool isCallValid,
                                         cl_command_queue command_queue,
                                         cl_mem buffer,
                                         cl_bool blocking_map,
                                         MapFlags map_flagsPacked,
                                         size_t offset,
                                         size_t size,
                                         cl_uint num_events_in_wait_list,
                                         const cl_event *event_wait_list,
                                         cl_event *event,
                                         cl_int *errcode_ret,
                                         angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureEnqueueMapImage_origin(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem image,
                                   cl_bool blocking_map,
                                   MapFlags map_flagsPacked,
                                   const size_t *origin,
                                   const size_t *region,
                                   size_t *image_row_pitch,
                                   size_t *image_slice_pitch,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    if (origin)
    {
        CaptureMemory(origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueMapImage_region(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem image,
                                   cl_bool blocking_map,
                                   MapFlags map_flagsPacked,
                                   const size_t *origin,
                                   const size_t *region,
                                   size_t *image_row_pitch,
                                   size_t *image_slice_pitch,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueMapImage_image_row_pitch(bool isCallValid,
                                            cl_command_queue command_queue,
                                            cl_mem image,
                                            cl_bool blocking_map,
                                            MapFlags map_flagsPacked,
                                            const size_t *origin,
                                            const size_t *region,
                                            size_t *image_row_pitch,
                                            size_t *image_slice_pitch,
                                            cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list,
                                            cl_event *event,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (image_row_pitch)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureEnqueueMapImage_image_slice_pitch(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_mem image,
                                              cl_bool blocking_map,
                                              MapFlags map_flagsPacked,
                                              const size_t *origin,
                                              const size_t *region,
                                              size_t *image_row_pitch,
                                              size_t *image_slice_pitch,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              cl_int *errcode_ret,
                                              angle::ParamCapture *paramCapture)
{
    if (image_slice_pitch)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureEnqueueMapImage_event_wait_list(bool isCallValid,
                                            cl_command_queue command_queue,
                                            cl_mem image,
                                            cl_bool blocking_map,
                                            MapFlags map_flagsPacked,
                                            const size_t *origin,
                                            const size_t *region,
                                            size_t *image_row_pitch,
                                            size_t *image_slice_pitch,
                                            cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list,
                                            cl_event *event,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueMapImage_event(bool isCallValid,
                                  cl_command_queue command_queue,
                                  cl_mem image,
                                  cl_bool blocking_map,
                                  MapFlags map_flagsPacked,
                                  const size_t *origin,
                                  const size_t *region,
                                  size_t *image_row_pitch,
                                  size_t *image_slice_pitch,
                                  cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list,
                                  cl_event *event,
                                  cl_int *errcode_ret,
                                  angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueMapImage_errcode_ret(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem image,
                                        cl_bool blocking_map,
                                        MapFlags map_flagsPacked,
                                        const size_t *origin,
                                        const size_t *region,
                                        size_t *image_row_pitch,
                                        size_t *image_slice_pitch,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        cl_int *errcode_ret,
                                        angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureEnqueueUnmapMemObject_mapped_ptr(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem memobj,
                                             void *mapped_ptr,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureEnqueueUnmapMemObject_event_wait_list(bool isCallValid,
                                                  cl_command_queue command_queue,
                                                  cl_mem memobj,
                                                  void *mapped_ptr,
                                                  cl_uint num_events_in_wait_list,
                                                  const cl_event *event_wait_list,
                                                  cl_event *event,
                                                  angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueUnmapMemObject_event(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem memobj,
                                        void *mapped_ptr,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueNDRangeKernel_global_work_offset(bool isCallValid,
                                                    cl_command_queue command_queue,
                                                    cl_kernel kernel,
                                                    cl_uint work_dim,
                                                    const size_t *global_work_offset,
                                                    const size_t *global_work_size,
                                                    const size_t *local_work_size,
                                                    cl_uint num_events_in_wait_list,
                                                    const cl_event *event_wait_list,
                                                    cl_event *event,
                                                    angle::ParamCapture *paramCapture)
{
    if (global_work_offset)
    {
        CaptureMemory(global_work_offset, work_dim * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueNDRangeKernel_global_work_size(bool isCallValid,
                                                  cl_command_queue command_queue,
                                                  cl_kernel kernel,
                                                  cl_uint work_dim,
                                                  const size_t *global_work_offset,
                                                  const size_t *global_work_size,
                                                  const size_t *local_work_size,
                                                  cl_uint num_events_in_wait_list,
                                                  const cl_event *event_wait_list,
                                                  cl_event *event,
                                                  angle::ParamCapture *paramCapture)
{
    if (global_work_size)
    {
        CaptureMemory(global_work_size, work_dim * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueNDRangeKernel_local_work_size(bool isCallValid,
                                                 cl_command_queue command_queue,
                                                 cl_kernel kernel,
                                                 cl_uint work_dim,
                                                 const size_t *global_work_offset,
                                                 const size_t *global_work_size,
                                                 const size_t *local_work_size,
                                                 cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list,
                                                 cl_event *event,
                                                 angle::ParamCapture *paramCapture)
{
    if (local_work_size)
    {
        CaptureMemory(local_work_size, work_dim * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueNDRangeKernel_event_wait_list(bool isCallValid,
                                                 cl_command_queue command_queue,
                                                 cl_kernel kernel,
                                                 cl_uint work_dim,
                                                 const size_t *global_work_offset,
                                                 const size_t *global_work_size,
                                                 const size_t *local_work_size,
                                                 cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list,
                                                 cl_event *event,
                                                 angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueNDRangeKernel_event(bool isCallValid,
                                       cl_command_queue command_queue,
                                       cl_kernel kernel,
                                       cl_uint work_dim,
                                       const size_t *global_work_offset,
                                       const size_t *global_work_size,
                                       const size_t *local_work_size,
                                       cl_uint num_events_in_wait_list,
                                       const cl_event *event_wait_list,
                                       cl_event *event,
                                       angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueNativeKernel_user_func(bool isCallValid,
                                          cl_command_queue command_queue,
                                          void(CL_CALLBACK *user_func)(void *),
                                          void *args,
                                          size_t cb_args,
                                          cl_uint num_mem_objects,
                                          const cl_mem *mem_list,
                                          const void **args_mem_loc,
                                          cl_uint num_events_in_wait_list,
                                          const cl_event *event_wait_list,
                                          cl_event *event,
                                          angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureEnqueueNativeKernel_args(bool isCallValid,
                                     cl_command_queue command_queue,
                                     void(CL_CALLBACK *user_func)(void *),
                                     void *args,
                                     size_t cb_args,
                                     cl_uint num_mem_objects,
                                     const cl_mem *mem_list,
                                     const void **args_mem_loc,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{

    // Store all binary data
    // Then do:
    // memcpy(args, binary_data[x], size)
    // args[x] = clMemMap[y];
    // ...
    // ...
    CaptureMemory(args, cb_args, paramCapture);
}
void CaptureEnqueueNativeKernel_mem_list(bool isCallValid,
                                         cl_command_queue command_queue,
                                         void(CL_CALLBACK *user_func)(void *),
                                         void *args,
                                         size_t cb_args,
                                         cl_uint num_mem_objects,
                                         const cl_mem *mem_list,
                                         const void **args_mem_loc,
                                         cl_uint num_events_in_wait_list,
                                         const cl_event *event_wait_list,
                                         cl_event *event,
                                         angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        mem_list, num_mem_objects, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureEnqueueNativeKernel_args_mem_loc(bool isCallValid,
                                             cl_command_queue command_queue,
                                             void(CL_CALLBACK *user_func)(void *),
                                             void *args,
                                             size_t cb_args,
                                             cl_uint num_mem_objects,
                                             const cl_mem *mem_list,
                                             const void **args_mem_loc,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setOffsetsVector(
        args, args_mem_loc, num_mem_objects, paramCapture);
}
void CaptureEnqueueNativeKernel_event_wait_list(bool isCallValid,
                                                cl_command_queue command_queue,
                                                void(CL_CALLBACK *user_func)(void *),
                                                void *args,
                                                size_t cb_args,
                                                cl_uint num_mem_objects,
                                                const cl_mem *mem_list,
                                                const void **args_mem_loc,
                                                cl_uint num_events_in_wait_list,
                                                const cl_event *event_wait_list,
                                                cl_event *event,
                                                angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueNativeKernel_event(bool isCallValid,
                                      cl_command_queue command_queue,
                                      void(CL_CALLBACK *user_func)(void *),
                                      void *args,
                                      size_t cb_args,
                                      cl_uint num_mem_objects,
                                      const cl_mem *mem_list,
                                      const void **args_mem_loc,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list,
                                      cl_event *event,
                                      angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureSetCommandQueueProperty_old_properties(bool isCallValid,
                                                   cl_command_queue command_queue,
                                                   CommandQueueProperties propertiesPacked,
                                                   cl_bool enable,
                                                   cl_command_queue_properties *old_properties,
                                                   angle::ParamCapture *paramCapture)
{
    if (old_properties)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_command_queue_properties);
    }
}

size_t GetCorrectedImageRowPitch(const cl_image_format *image_format,
                                 size_t image_row_pitch,
                                 size_t image_width)
{
    size_t corrected_image_row_pitch = image_row_pitch;
    if (corrected_image_row_pitch == 0)
    {
        switch (image_format->image_channel_order)
        {
            case CL_R:
            case CL_A:
            case CL_DEPTH:
            case CL_LUMINANCE:
            case CL_INTENSITY:
                corrected_image_row_pitch = 1;
                break;
            case CL_RG:
            case CL_RA:
            case CL_Rx:
            case CL_DEPTH_STENCIL:
                corrected_image_row_pitch = 2;
                break;
            case CL_RGB:
            case CL_RGx:
            case CL_sRGB:
                corrected_image_row_pitch = 3;
                break;
            case CL_RGBA:
            case CL_BGRA:
            case CL_ARGB:
            case CL_ABGR:
            case CL_RGBx:
            case CL_sRGBA:
            case CL_sBGRA:
            case CL_sRGBx:
                corrected_image_row_pitch = 4;
                break;
            default:
                break;
        }

        switch (image_format->image_channel_data_type)
        {
            case CL_SNORM_INT16:
            case CL_UNORM_INT16:
            case CL_UNORM_SHORT_565:
            case CL_UNORM_SHORT_555:
            case CL_SIGNED_INT16:
            case CL_UNSIGNED_INT16:
            case CL_HALF_FLOAT:
                corrected_image_row_pitch *= 2;
                break;
            case CL_UNORM_INT24:
                corrected_image_row_pitch *= 3;
                break;
            case CL_UNORM_INT_101010:
            case CL_UNORM_INT_101010_2:
            case CL_FLOAT:
            case CL_SIGNED_INT32:
            case CL_UNSIGNED_INT32:
                corrected_image_row_pitch *= 4;
                break;
            default:
                break;
        }

        corrected_image_row_pitch *= image_width;
    }
    return corrected_image_row_pitch;
}

size_t GetCorrectedImageSlicePitch(size_t image_row_pitch,
                                   size_t image_slice_pitch,
                                   size_t image_height,
                                   cl_mem_object_type image_type)
{
    size_t corrected_image_slice_pitch = image_slice_pitch;
    if (corrected_image_slice_pitch == 0)
    {
        switch (image_type)
        {
            case CL_MEM_OBJECT_IMAGE3D:
            case CL_MEM_OBJECT_IMAGE2D_ARRAY:
                corrected_image_slice_pitch = image_row_pitch * image_height;
                break;
            case CL_MEM_OBJECT_IMAGE1D_ARRAY:
                corrected_image_slice_pitch = image_row_pitch;
                break;
            default:
                break;
        }
    }
    return corrected_image_slice_pitch;
}

void CaptureCreateImage2D_image_format(bool isCallValid,
                                       cl_context context,
                                       MemFlags flagsPacked,
                                       const cl_image_format *image_format,
                                       size_t image_width,
                                       size_t image_height,
                                       size_t image_row_pitch,
                                       void *host_ptr,
                                       cl_int *errcode_ret,
                                       angle::ParamCapture *paramCapture)
{
    if (image_format)
    {
        CaptureMemory(image_format, sizeof(cl_image_format), paramCapture);
    }
}
void CaptureCreateImage2D_host_ptr(bool isCallValid,
                                   cl_context context,
                                   MemFlags flagsPacked,
                                   const cl_image_format *image_format,
                                   size_t image_width,
                                   size_t image_height,
                                   size_t image_row_pitch,
                                   void *host_ptr,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    CaptureMemory(
        host_ptr,
        GetCorrectedImageRowPitch(image_format, image_row_pitch, image_width) * image_height,
        paramCapture);
}
void CaptureCreateImage2D_errcode_ret(bool isCallValid,
                                      cl_context context,
                                      MemFlags flagsPacked,
                                      const cl_image_format *image_format,
                                      size_t image_width,
                                      size_t image_height,
                                      size_t image_row_pitch,
                                      void *host_ptr,
                                      cl_int *errcode_ret,
                                      angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateImage3D_image_format(bool isCallValid,
                                       cl_context context,
                                       MemFlags flagsPacked,
                                       const cl_image_format *image_format,
                                       size_t image_width,
                                       size_t image_height,
                                       size_t image_depth,
                                       size_t image_row_pitch,
                                       size_t image_slice_pitch,
                                       void *host_ptr,
                                       cl_int *errcode_ret,
                                       angle::ParamCapture *paramCapture)
{
    if (image_format)
    {
        CaptureMemory(image_format, sizeof(cl_image_format), paramCapture);
    }
}
void CaptureCreateImage3D_host_ptr(bool isCallValid,
                                   cl_context context,
                                   MemFlags flagsPacked,
                                   const cl_image_format *image_format,
                                   size_t image_width,
                                   size_t image_height,
                                   size_t image_depth,
                                   size_t image_row_pitch,
                                   size_t image_slice_pitch,
                                   void *host_ptr,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    size_t correctedImageRowPitch =
        GetCorrectedImageRowPitch(image_format, image_row_pitch, image_width);
    CaptureMemory(host_ptr,
                  GetCorrectedImageSlicePitch(correctedImageRowPitch, image_slice_pitch,
                                              image_height, CL_MEM_OBJECT_IMAGE3D) *
                      image_depth,
                  paramCapture);
}
void CaptureCreateImage3D_errcode_ret(bool isCallValid,
                                      cl_context context,
                                      MemFlags flagsPacked,
                                      const cl_image_format *image_format,
                                      size_t image_width,
                                      size_t image_height,
                                      size_t image_depth,
                                      size_t image_row_pitch,
                                      size_t image_slice_pitch,
                                      void *host_ptr,
                                      cl_int *errcode_ret,
                                      angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureEnqueueMarker_event(bool isCallValid,
                                cl_command_queue command_queue,
                                cl_event *event,
                                angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueWaitForEvents_event_list(bool isCallValid,
                                            cl_command_queue command_queue,
                                            cl_uint num_events,
                                            const cl_event *event_list,
                                            angle::ParamCapture *paramCapture)
{
    if (event_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_list, num_events, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureGetExtensionFunctionAddress_func_name(bool isCallValid,
                                                  const char *func_name,
                                                  angle::ParamCapture *paramCapture)
{
    if (func_name)
    {
        CaptureString(func_name, paramCapture);
    }
}
void CaptureCreateCommandQueue_errcode_ret(bool isCallValid,
                                           cl_context context,
                                           cl_device_id device,
                                           CommandQueueProperties propertiesPacked,
                                           cl_int *errcode_ret,
                                           angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateSampler_errcode_ret(bool isCallValid,
                                      cl_context context,
                                      cl_bool normalized_coords,
                                      AddressingMode addressing_modePacked,
                                      FilterMode filter_modePacked,
                                      cl_int *errcode_ret,
                                      angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureEnqueueTask_event_wait_list(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_kernel kernel,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueTask_event(bool isCallValid,
                              cl_command_queue command_queue,
                              cl_kernel kernel,
                              cl_uint num_events_in_wait_list,
                              const cl_event *event_wait_list,
                              cl_event *event,
                              angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureCreateSubBuffer_buffer_create_info(bool isCallValid,
                                               cl_mem buffer,
                                               MemFlags flagsPacked,
                                               cl_buffer_create_type buffer_create_type,
                                               const void *buffer_create_info,
                                               cl_int *errcode_ret,
                                               angle::ParamCapture *paramCapture)
{
    if (buffer_create_info)
    {
        CaptureMemory(buffer_create_info, sizeof(cl_buffer_region), paramCapture);
    }
}
void CaptureCreateSubBuffer_errcode_ret(bool isCallValid,
                                        cl_mem buffer,
                                        MemFlags flagsPacked,
                                        cl_buffer_create_type buffer_create_type,
                                        const void *buffer_create_info,
                                        cl_int *errcode_ret,
                                        angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureSetMemObjectDestructorCallback_pfn_notify(
    bool isCallValid,
    cl_mem memobj,
    void(CL_CALLBACK *pfn_notify)(cl_mem memobj, void *user_data),
    void *user_data,
    angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureSetMemObjectDestructorCallback_user_data(bool isCallValid,
                                                     cl_mem memobj,
                                                     void(CL_CALLBACK *pfn_notify)(cl_mem memobj,
                                                                                   void *user_data),
                                                     void *user_data,
                                                     angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureCreateUserEvent_errcode_ret(bool isCallValid,
                                        cl_context context,
                                        cl_int *errcode_ret,
                                        angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureSetEventCallback_pfn_notify(bool isCallValid,
                                        cl_event event,
                                        cl_int command_exec_callback_type,
                                        void(CL_CALLBACK *pfn_notify)(cl_event event,
                                                                      cl_int event_command_status,
                                                                      void *user_data),
                                        void *user_data,
                                        angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureSetEventCallback_user_data(bool isCallValid,
                                       cl_event event,
                                       cl_int command_exec_callback_type,
                                       void(CL_CALLBACK *pfn_notify)(cl_event event,
                                                                     cl_int event_command_status,
                                                                     void *user_data),
                                       void *user_data,
                                       angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureEnqueueReadBufferRect_buffer_origin(bool isCallValid,
                                                cl_command_queue command_queue,
                                                cl_mem buffer,
                                                cl_bool blocking_read,
                                                const size_t *buffer_origin,
                                                const size_t *host_origin,
                                                const size_t *region,
                                                size_t buffer_row_pitch,
                                                size_t buffer_slice_pitch,
                                                size_t host_row_pitch,
                                                size_t host_slice_pitch,
                                                void *ptr,
                                                cl_uint num_events_in_wait_list,
                                                const cl_event *event_wait_list,
                                                cl_event *event,
                                                angle::ParamCapture *paramCapture)
{
    if (buffer_origin)
    {
        CaptureMemory(buffer_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueReadBufferRect_host_origin(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_mem buffer,
                                              cl_bool blocking_read,
                                              const size_t *buffer_origin,
                                              const size_t *host_origin,
                                              const size_t *region,
                                              size_t buffer_row_pitch,
                                              size_t buffer_slice_pitch,
                                              size_t host_row_pitch,
                                              size_t host_slice_pitch,
                                              void *ptr,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (host_origin)
    {
        CaptureMemory(host_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueReadBufferRect_region(bool isCallValid,
                                         cl_command_queue command_queue,
                                         cl_mem buffer,
                                         cl_bool blocking_read,
                                         const size_t *buffer_origin,
                                         const size_t *host_origin,
                                         const size_t *region,
                                         size_t buffer_row_pitch,
                                         size_t buffer_slice_pitch,
                                         size_t host_row_pitch,
                                         size_t host_slice_pitch,
                                         void *ptr,
                                         cl_uint num_events_in_wait_list,
                                         const cl_event *event_wait_list,
                                         cl_event *event,
                                         angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueReadBufferRect_ptr(bool isCallValid,
                                      cl_command_queue command_queue,
                                      cl_mem buffer,
                                      cl_bool blocking_read,
                                      const size_t *buffer_origin,
                                      const size_t *host_origin,
                                      const size_t *region,
                                      size_t buffer_row_pitch,
                                      size_t buffer_slice_pitch,
                                      size_t host_row_pitch,
                                      size_t host_slice_pitch,
                                      void *ptr,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list,
                                      cl_event *event,
                                      angle::ParamCapture *paramCapture)
{
    if (ptr)
    {
        // According to docs, "If host_row_pitch is 0, host_row_pitch is computed as region[0]" and
        // "If host_slice_pitch is 0, host_slice_pitch is computed as region[1] x host_row_pitch"
        size_t computed_host_row_pitch = host_row_pitch != 0 ? host_row_pitch : region[0];
        size_t computed_host_slice_pitch =
            host_slice_pitch != 0 ? host_slice_pitch : computed_host_row_pitch * region[1];

        // According to docs, "The offset in bytes is computed as host_origin[2] x host_slice_pitch
        // + host_origin[1] x host_row_pitch + host_origin[0]"
        size_t totalOffset = (host_origin[2] * computed_host_slice_pitch +
                              host_origin[1] * computed_host_row_pitch + host_origin[0]);

        // Total size = (total offset in bytes) + (total size in bytes of desired memory including
        // padding)
        paramCapture->readBufferSizeBytes = totalOffset +
                                            (region[2] - 1) * computed_host_slice_pitch +
                                            (region[1] - 1) * computed_host_row_pitch + region[0];
    }
}
void CaptureEnqueueReadBufferRect_event_wait_list(bool isCallValid,
                                                  cl_command_queue command_queue,
                                                  cl_mem buffer,
                                                  cl_bool blocking_read,
                                                  const size_t *buffer_origin,
                                                  const size_t *host_origin,
                                                  const size_t *region,
                                                  size_t buffer_row_pitch,
                                                  size_t buffer_slice_pitch,
                                                  size_t host_row_pitch,
                                                  size_t host_slice_pitch,
                                                  void *ptr,
                                                  cl_uint num_events_in_wait_list,
                                                  const cl_event *event_wait_list,
                                                  cl_event *event,
                                                  angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueReadBufferRect_event(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem buffer,
                                        cl_bool blocking_read,
                                        const size_t *buffer_origin,
                                        const size_t *host_origin,
                                        const size_t *region,
                                        size_t buffer_row_pitch,
                                        size_t buffer_slice_pitch,
                                        size_t host_row_pitch,
                                        size_t host_slice_pitch,
                                        void *ptr,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueWriteBufferRect_buffer_origin(bool isCallValid,
                                                 cl_command_queue command_queue,
                                                 cl_mem buffer,
                                                 cl_bool blocking_write,
                                                 const size_t *buffer_origin,
                                                 const size_t *host_origin,
                                                 const size_t *region,
                                                 size_t buffer_row_pitch,
                                                 size_t buffer_slice_pitch,
                                                 size_t host_row_pitch,
                                                 size_t host_slice_pitch,
                                                 const void *ptr,
                                                 cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list,
                                                 cl_event *event,
                                                 angle::ParamCapture *paramCapture)
{
    if (buffer_origin)
    {
        CaptureMemory(buffer_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueWriteBufferRect_host_origin(bool isCallValid,
                                               cl_command_queue command_queue,
                                               cl_mem buffer,
                                               cl_bool blocking_write,
                                               const size_t *buffer_origin,
                                               const size_t *host_origin,
                                               const size_t *region,
                                               size_t buffer_row_pitch,
                                               size_t buffer_slice_pitch,
                                               size_t host_row_pitch,
                                               size_t host_slice_pitch,
                                               const void *ptr,
                                               cl_uint num_events_in_wait_list,
                                               const cl_event *event_wait_list,
                                               cl_event *event,
                                               angle::ParamCapture *paramCapture)
{
    if (host_origin)
    {
        CaptureMemory(host_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueWriteBufferRect_region(bool isCallValid,
                                          cl_command_queue command_queue,
                                          cl_mem buffer,
                                          cl_bool blocking_write,
                                          const size_t *buffer_origin,
                                          const size_t *host_origin,
                                          const size_t *region,
                                          size_t buffer_row_pitch,
                                          size_t buffer_slice_pitch,
                                          size_t host_row_pitch,
                                          size_t host_slice_pitch,
                                          const void *ptr,
                                          cl_uint num_events_in_wait_list,
                                          const cl_event *event_wait_list,
                                          cl_event *event,
                                          angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueWriteBufferRect_ptr(bool isCallValid,
                                       cl_command_queue command_queue,
                                       cl_mem buffer,
                                       cl_bool blocking_write,
                                       const size_t *buffer_origin,
                                       const size_t *host_origin,
                                       const size_t *region,
                                       size_t buffer_row_pitch,
                                       size_t buffer_slice_pitch,
                                       size_t host_row_pitch,
                                       size_t host_slice_pitch,
                                       const void *ptr,
                                       cl_uint num_events_in_wait_list,
                                       const cl_event *event_wait_list,
                                       cl_event *event,
                                       angle::ParamCapture *paramCapture)
{
    if (ptr)
    {
        // According to docs, "If host_row_pitch is 0, host_row_pitch is computed as region[0]" and
        // "If host_slice_pitch is 0, host_slice_pitch is computed as region[1] x host_row_pitch"
        size_t computed_host_row_pitch = host_row_pitch != 0 ? host_row_pitch : region[0];
        size_t computed_host_slice_pitch =
            host_slice_pitch != 0 ? host_slice_pitch : computed_host_row_pitch * region[1];

        // According to docs, "The offset in bytes is computed as host_origin[2] x host_slice_pitch
        // + host_origin[1] x host_row_pitch + host_origin[0]"
        size_t totalOffset = (host_origin[2] * computed_host_slice_pitch +
                              host_origin[1] * computed_host_row_pitch + host_origin[0]);

        // total size = (total offset in bytes) + (total size in bytes of desired memory including
        // padding)
        size_t totalSize = totalOffset + (region[2] - 1) * computed_host_slice_pitch +
                           (region[1] - 1) * computed_host_row_pitch + region[0];
        CaptureMemory(ptr, totalSize, paramCapture);
    }
}
void CaptureEnqueueWriteBufferRect_event_wait_list(bool isCallValid,
                                                   cl_command_queue command_queue,
                                                   cl_mem buffer,
                                                   cl_bool blocking_write,
                                                   const size_t *buffer_origin,
                                                   const size_t *host_origin,
                                                   const size_t *region,
                                                   size_t buffer_row_pitch,
                                                   size_t buffer_slice_pitch,
                                                   size_t host_row_pitch,
                                                   size_t host_slice_pitch,
                                                   const void *ptr,
                                                   cl_uint num_events_in_wait_list,
                                                   const cl_event *event_wait_list,
                                                   cl_event *event,
                                                   angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueWriteBufferRect_event(bool isCallValid,
                                         cl_command_queue command_queue,
                                         cl_mem buffer,
                                         cl_bool blocking_write,
                                         const size_t *buffer_origin,
                                         const size_t *host_origin,
                                         const size_t *region,
                                         size_t buffer_row_pitch,
                                         size_t buffer_slice_pitch,
                                         size_t host_row_pitch,
                                         size_t host_slice_pitch,
                                         const void *ptr,
                                         cl_uint num_events_in_wait_list,
                                         const cl_event *event_wait_list,
                                         cl_event *event,
                                         angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueCopyBufferRect_src_origin(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem src_buffer,
                                             cl_mem dst_buffer,
                                             const size_t *src_origin,
                                             const size_t *dst_origin,
                                             const size_t *region,
                                             size_t src_row_pitch,
                                             size_t src_slice_pitch,
                                             size_t dst_row_pitch,
                                             size_t dst_slice_pitch,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (src_origin)
    {
        CaptureMemory(src_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyBufferRect_dst_origin(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem src_buffer,
                                             cl_mem dst_buffer,
                                             const size_t *src_origin,
                                             const size_t *dst_origin,
                                             const size_t *region,
                                             size_t src_row_pitch,
                                             size_t src_slice_pitch,
                                             size_t dst_row_pitch,
                                             size_t dst_slice_pitch,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (dst_origin)
    {
        CaptureMemory(dst_origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyBufferRect_region(bool isCallValid,
                                         cl_command_queue command_queue,
                                         cl_mem src_buffer,
                                         cl_mem dst_buffer,
                                         const size_t *src_origin,
                                         const size_t *dst_origin,
                                         const size_t *region,
                                         size_t src_row_pitch,
                                         size_t src_slice_pitch,
                                         size_t dst_row_pitch,
                                         size_t dst_slice_pitch,
                                         cl_uint num_events_in_wait_list,
                                         const cl_event *event_wait_list,
                                         cl_event *event,
                                         angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueCopyBufferRect_event_wait_list(bool isCallValid,
                                                  cl_command_queue command_queue,
                                                  cl_mem src_buffer,
                                                  cl_mem dst_buffer,
                                                  const size_t *src_origin,
                                                  const size_t *dst_origin,
                                                  const size_t *region,
                                                  size_t src_row_pitch,
                                                  size_t src_slice_pitch,
                                                  size_t dst_row_pitch,
                                                  size_t dst_slice_pitch,
                                                  cl_uint num_events_in_wait_list,
                                                  const cl_event *event_wait_list,
                                                  cl_event *event,
                                                  angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueCopyBufferRect_event(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem src_buffer,
                                        cl_mem dst_buffer,
                                        const size_t *src_origin,
                                        const size_t *dst_origin,
                                        const size_t *region,
                                        size_t src_row_pitch,
                                        size_t src_slice_pitch,
                                        size_t dst_row_pitch,
                                        size_t dst_slice_pitch,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureCreateSubDevices_properties(bool isCallValid,
                                        cl_device_id in_device,
                                        const cl_device_partition_property *properties,
                                        cl_uint num_devices,
                                        cl_device_id *out_devices,
                                        cl_uint *num_devices_ret,
                                        angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        size_t propertiesSize = 0;
        while (properties[propertiesSize++] != 0)
        {
        }
        CaptureMemory(properties, sizeof(cl_device_partition_property) * propertiesSize,
                      paramCapture);
    }
}
void CaptureCreateSubDevices_out_devices(bool isCallValid,
                                         cl_device_id in_device,
                                         const cl_device_partition_property *properties,
                                         cl_uint num_devices,
                                         cl_device_id *out_devices,
                                         cl_uint *num_devices_ret,
                                         angle::ParamCapture *paramCapture)
{
    if (out_devices)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLDeviceIndices(out_devices,
                                                                                num_devices);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            out_devices, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureCreateSubDevices_num_devices_ret(bool isCallValid,
                                             cl_device_id in_device,
                                             const cl_device_partition_property *properties,
                                             cl_uint num_devices,
                                             cl_device_id *out_devices,
                                             cl_uint *num_devices_ret,
                                             angle::ParamCapture *paramCapture)
{
    if (num_devices_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_uint);
    }
}
void CaptureCreateImage_image_format(bool isCallValid,
                                     cl_context context,
                                     MemFlags flagsPacked,
                                     const cl_image_format *image_format,
                                     const cl_image_desc *image_desc,
                                     void *host_ptr,
                                     cl_int *errcode_ret,
                                     angle::ParamCapture *paramCapture)
{
    if (image_format)
    {
        CaptureMemory(image_format, sizeof(cl_image_format), paramCapture);
    }
}
void CaptureCreateImage_image_desc(bool isCallValid,
                                   cl_context context,
                                   MemFlags flagsPacked,
                                   const cl_image_format *image_format,
                                   const cl_image_desc *image_desc,
                                   void *host_ptr,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    if (image_desc)
    {
        CaptureMemory(image_desc, sizeof(cl_image_desc), paramCapture);
    }
}
void CaptureCreateImage_host_ptr(bool isCallValid,
                                 cl_context context,
                                 MemFlags flagsPacked,
                                 const cl_image_format *image_format,
                                 const cl_image_desc *image_desc,
                                 void *host_ptr,
                                 cl_int *errcode_ret,
                                 angle::ParamCapture *paramCapture)
{
    if (host_ptr && image_desc)
    {
        size_t image_size             = 0;
        size_t correctedImageRowPitch = GetCorrectedImageRowPitch(
            image_format, image_desc->image_row_pitch, image_desc->image_width);
        size_t correctedImageSlicePitch =
            GetCorrectedImageSlicePitch(correctedImageRowPitch, image_desc->image_slice_pitch,
                                        image_desc->image_height, image_desc->image_type);

        switch (image_desc->image_type)
        {
            case CL_MEM_OBJECT_IMAGE1D:
            case CL_MEM_OBJECT_IMAGE1D_BUFFER:
                image_size = correctedImageRowPitch;
                break;
            case CL_MEM_OBJECT_IMAGE2D:
                image_size = correctedImageRowPitch * image_desc->image_height;
                break;
            case CL_MEM_OBJECT_IMAGE3D:
                image_size = correctedImageSlicePitch * image_desc->image_depth;
                break;
            case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            case CL_MEM_OBJECT_IMAGE2D_ARRAY:
                image_size = correctedImageSlicePitch * image_desc->image_array_size;
                break;
            default:
                break;
        }

        CaptureMemory(host_ptr, image_size, paramCapture);
    }
}
void CaptureCreateImage_errcode_ret(bool isCallValid,
                                    cl_context context,
                                    MemFlags flagsPacked,
                                    const cl_image_format *image_format,
                                    const cl_image_desc *image_desc,
                                    void *host_ptr,
                                    cl_int *errcode_ret,
                                    angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateProgramWithBuiltInKernels_device_list(bool isCallValid,
                                                        cl_context context,
                                                        cl_uint num_devices,
                                                        const cl_device_id *device_list,
                                                        const char *kernel_names,
                                                        cl_int *errcode_ret,
                                                        angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        device_list, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureCreateProgramWithBuiltInKernels_kernel_names(bool isCallValid,
                                                         cl_context context,
                                                         cl_uint num_devices,
                                                         const cl_device_id *device_list,
                                                         const char *kernel_names,
                                                         cl_int *errcode_ret,
                                                         angle::ParamCapture *paramCapture)
{
    if (kernel_names)
    {
        CaptureString(kernel_names, paramCapture);
    }
}
void CaptureCreateProgramWithBuiltInKernels_errcode_ret(bool isCallValid,
                                                        cl_context context,
                                                        cl_uint num_devices,
                                                        const cl_device_id *device_list,
                                                        const char *kernel_names,
                                                        cl_int *errcode_ret,
                                                        angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCompileProgram_device_list(bool isCallValid,
                                       cl_program program,
                                       cl_uint num_devices,
                                       const cl_device_id *device_list,
                                       const char *options,
                                       cl_uint num_input_headers,
                                       const cl_program *input_headers,
                                       const char **header_include_names,
                                       void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                     void *user_data),
                                       void *user_data,
                                       angle::ParamCapture *paramCapture)
{
    if (device_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            device_list, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureCompileProgram_options(bool isCallValid,
                                   cl_program program,
                                   cl_uint num_devices,
                                   const cl_device_id *device_list,
                                   const char *options,
                                   cl_uint num_input_headers,
                                   const cl_program *input_headers,
                                   const char **header_include_names,
                                   void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                 void *user_data),
                                   void *user_data,
                                   angle::ParamCapture *paramCapture)
{
    if (options)
    {
        CaptureString(options, paramCapture);
    }
}
void CaptureCompileProgram_input_headers(bool isCallValid,
                                         cl_program program,
                                         cl_uint num_devices,
                                         const cl_device_id *device_list,
                                         const char *options,
                                         cl_uint num_input_headers,
                                         const cl_program *input_headers,
                                         const char **header_include_names,
                                         void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                       void *user_data),
                                         void *user_data,
                                         angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        input_headers, num_input_headers, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureCompileProgram_header_include_names(bool isCallValid,
                                                cl_program program,
                                                cl_uint num_devices,
                                                const cl_device_id *device_list,
                                                const char *options,
                                                cl_uint num_input_headers,
                                                const cl_program *input_headers,
                                                const char **header_include_names,
                                                void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                              void *user_data),
                                                void *user_data,
                                                angle::ParamCapture *paramCapture)
{
    for (size_t i = 0; i < num_input_headers; ++i)
    {
        CaptureMemory(header_include_names[i], (strlen(header_include_names[i]) + 1) * sizeof(char),
                      paramCapture);
    }
}
void CaptureCompileProgram_pfn_notify(bool isCallValid,
                                      cl_program program,
                                      cl_uint num_devices,
                                      const cl_device_id *device_list,
                                      const char *options,
                                      cl_uint num_input_headers,
                                      const cl_program *input_headers,
                                      const char **header_include_names,
                                      void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                    void *user_data),
                                      void *user_data,
                                      angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureCompileProgram_user_data(bool isCallValid,
                                     cl_program program,
                                     cl_uint num_devices,
                                     const cl_device_id *device_list,
                                     const char *options,
                                     cl_uint num_input_headers,
                                     const cl_program *input_headers,
                                     const char **header_include_names,
                                     void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                   void *user_data),
                                     void *user_data,
                                     angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureLinkProgram_device_list(bool isCallValid,
                                    cl_context context,
                                    cl_uint num_devices,
                                    const cl_device_id *device_list,
                                    const char *options,
                                    cl_uint num_input_programs,
                                    const cl_program *input_programs,
                                    void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                  void *user_data),
                                    void *user_data,
                                    cl_int *errcode_ret,
                                    angle::ParamCapture *paramCapture)
{
    if (device_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            device_list, num_devices, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureLinkProgram_options(bool isCallValid,
                                cl_context context,
                                cl_uint num_devices,
                                const cl_device_id *device_list,
                                const char *options,
                                cl_uint num_input_programs,
                                const cl_program *input_programs,
                                void(CL_CALLBACK *pfn_notify)(cl_program program, void *user_data),
                                void *user_data,
                                cl_int *errcode_ret,
                                angle::ParamCapture *paramCapture)
{
    if (options)
    {
        CaptureString(options, paramCapture);
    }
}
void CaptureLinkProgram_input_programs(bool isCallValid,
                                       cl_context context,
                                       cl_uint num_devices,
                                       const cl_device_id *device_list,
                                       const char *options,
                                       cl_uint num_input_programs,
                                       const cl_program *input_programs,
                                       void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                     void *user_data),
                                       void *user_data,
                                       cl_int *errcode_ret,
                                       angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        input_programs, num_input_programs, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureLinkProgram_pfn_notify(bool isCallValid,
                                   cl_context context,
                                   cl_uint num_devices,
                                   const cl_device_id *device_list,
                                   const char *options,
                                   cl_uint num_input_programs,
                                   const cl_program *input_programs,
                                   void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                 void *user_data),
                                   void *user_data,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureLinkProgram_user_data(bool isCallValid,
                                  cl_context context,
                                  cl_uint num_devices,
                                  const cl_device_id *device_list,
                                  const char *options,
                                  cl_uint num_input_programs,
                                  const cl_program *input_programs,
                                  void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                void *user_data),
                                  void *user_data,
                                  cl_int *errcode_ret,
                                  angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureLinkProgram_errcode_ret(bool isCallValid,
                                    cl_context context,
                                    cl_uint num_devices,
                                    const cl_device_id *device_list,
                                    const char *options,
                                    cl_uint num_input_programs,
                                    const cl_program *input_programs,
                                    void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                  void *user_data),
                                    void *user_data,
                                    cl_int *errcode_ret,
                                    angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureGetKernelArgInfo_param_value(bool isCallValid,
                                         cl_kernel kernel,
                                         cl_uint arg_index,
                                         KernelArgInfo param_namePacked,
                                         size_t param_value_size,
                                         void *param_value,
                                         size_t *param_value_size_ret,
                                         angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetKernelArgInfo_param_value_size_ret(bool isCallValid,
                                                  cl_kernel kernel,
                                                  cl_uint arg_index,
                                                  KernelArgInfo param_namePacked,
                                                  size_t param_value_size,
                                                  void *param_value,
                                                  size_t *param_value_size_ret,
                                                  angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureEnqueueFillBuffer_pattern(bool isCallValid,
                                      cl_command_queue command_queue,
                                      cl_mem buffer,
                                      const void *pattern,
                                      size_t pattern_size,
                                      size_t offset,
                                      size_t size,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list,
                                      cl_event *event,
                                      angle::ParamCapture *paramCapture)
{
    if (pattern)
    {
        CaptureMemory(pattern, offset + size, paramCapture);
    }
}
void CaptureEnqueueFillBuffer_event_wait_list(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_mem buffer,
                                              const void *pattern,
                                              size_t pattern_size,
                                              size_t offset,
                                              size_t size,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueFillBuffer_event(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem buffer,
                                    const void *pattern,
                                    size_t pattern_size,
                                    size_t offset,
                                    size_t size,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueFillImage_fill_color(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_mem image,
                                        const void *fill_color,
                                        const size_t *origin,
                                        const size_t *region,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    void *value = (void *)malloc(sizeof(cl_image_format));
    if (IsError(
            image->cast<Image>().getInfo(ImageInfo::Format, sizeof(cl_image_format), value, NULL)))
    {
        return;
    }
    cl_image_format imageFormat = *(cl_image_format *)value;
    size_t totalSize;
    if (imageFormat.image_channel_order == CL_DEPTH)
    {
        totalSize = sizeof(cl_float);
    }
    else
    {
        switch (imageFormat.image_channel_data_type)
        {
            case CL_SNORM_INT8:
            case CL_UNORM_INT8:
            case CL_SIGNED_INT8:
            case CL_UNSIGNED_INT8:
                totalSize = 4 * sizeof(cl_uchar);
                break;
            case CL_SNORM_INT16:
            case CL_UNORM_INT16:
            case CL_SIGNED_INT16:
            case CL_UNSIGNED_INT16:
                totalSize = 4 * sizeof(cl_ushort);
                break;
            case CL_SIGNED_INT32:
                totalSize = 4 * sizeof(cl_int);
                break;
            case CL_UNSIGNED_INT32:
                totalSize = 4 * sizeof(cl_uint);
                break;
            case CL_HALF_FLOAT:
                totalSize = 4 * sizeof(cl_half);
                break;
            case CL_FLOAT:
                totalSize = 4 * sizeof(cl_float);
                break;
            case CL_UNORM_SHORT_565:
            case CL_UNORM_SHORT_555:
            case CL_UNORM_INT_101010:
                totalSize = 4 * sizeof(cl_ushort);
                break;
            default:
                totalSize = 0;
                break;
        }
    }

    CaptureMemory(fill_color, totalSize, paramCapture);
}
void CaptureEnqueueFillImage_origin(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem image,
                                    const void *fill_color,
                                    const size_t *origin,
                                    const size_t *region,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (origin)
    {
        CaptureMemory(origin, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueFillImage_region(bool isCallValid,
                                    cl_command_queue command_queue,
                                    cl_mem image,
                                    const void *fill_color,
                                    const size_t *origin,
                                    const size_t *region,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (region)
    {
        CaptureMemory(region, 3 * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueFillImage_event_wait_list(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_mem image,
                                             const void *fill_color,
                                             const size_t *origin,
                                             const size_t *region,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueFillImage_event(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_mem image,
                                   const void *fill_color,
                                   const size_t *origin,
                                   const size_t *region,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueMigrateMemObjects_mem_objects(bool isCallValid,
                                                 cl_command_queue command_queue,
                                                 cl_uint num_mem_objects,
                                                 const cl_mem *mem_objects,
                                                 MemMigrationFlags flagsPacked,
                                                 cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list,
                                                 cl_event *event,
                                                 angle::ParamCapture *paramCapture)
{
    cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
        mem_objects, num_mem_objects, paramCapture, &angle::FrameCaptureShared::getIndex);
}
void CaptureEnqueueMigrateMemObjects_event_wait_list(bool isCallValid,
                                                     cl_command_queue command_queue,
                                                     cl_uint num_mem_objects,
                                                     const cl_mem *mem_objects,
                                                     MemMigrationFlags flagsPacked,
                                                     cl_uint num_events_in_wait_list,
                                                     const cl_event *event_wait_list,
                                                     cl_event *event,
                                                     angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueMigrateMemObjects_event(bool isCallValid,
                                           cl_command_queue command_queue,
                                           cl_uint num_mem_objects,
                                           const cl_mem *mem_objects,
                                           MemMigrationFlags flagsPacked,
                                           cl_uint num_events_in_wait_list,
                                           const cl_event *event_wait_list,
                                           cl_event *event,
                                           angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueMarkerWithWaitList_event_wait_list(bool isCallValid,
                                                      cl_command_queue command_queue,
                                                      cl_uint num_events_in_wait_list,
                                                      const cl_event *event_wait_list,
                                                      cl_event *event,
                                                      angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueMarkerWithWaitList_event(bool isCallValid,
                                            cl_command_queue command_queue,
                                            cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list,
                                            cl_event *event,
                                            angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueBarrierWithWaitList_event_wait_list(bool isCallValid,
                                                       cl_command_queue command_queue,
                                                       cl_uint num_events_in_wait_list,
                                                       const cl_event *event_wait_list,
                                                       cl_event *event,
                                                       angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueBarrierWithWaitList_event(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureGetExtensionFunctionAddressForPlatform_func_name(bool isCallValid,
                                                             cl_platform_id platform,
                                                             const char *func_name,
                                                             angle::ParamCapture *paramCapture)
{
    if (func_name)
    {
        CaptureString(func_name, paramCapture);
    }
}
void CaptureCreateCommandQueueWithProperties_properties(bool isCallValid,
                                                        cl_context context,
                                                        cl_device_id device,
                                                        const cl_queue_properties *properties,
                                                        cl_int *errcode_ret,
                                                        angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        size_t propertiesSize = 0;
        while (properties[propertiesSize++] != 0)
        {
        }
        CaptureMemory((void *)properties, sizeof(cl_queue_properties) * propertiesSize,
                      paramCapture);
    }
}
void CaptureCreateCommandQueueWithProperties_errcode_ret(bool isCallValid,
                                                         cl_context context,
                                                         cl_device_id device,
                                                         const cl_queue_properties *properties,
                                                         cl_int *errcode_ret,
                                                         angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreatePipe_properties(bool isCallValid,
                                  cl_context context,
                                  MemFlags flagsPacked,
                                  cl_uint pipe_packet_size,
                                  cl_uint pipe_max_packets,
                                  const cl_pipe_properties *properties,
                                  cl_int *errcode_ret,
                                  angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        size_t propertiesSize = 0;
        while (properties[propertiesSize++] != 0)
        {
        }
        CaptureMemory((void *)properties, sizeof(cl_pipe_properties) * propertiesSize,
                      paramCapture);
    }
}
void CaptureCreatePipe_errcode_ret(bool isCallValid,
                                   cl_context context,
                                   MemFlags flagsPacked,
                                   cl_uint pipe_packet_size,
                                   cl_uint pipe_max_packets,
                                   const cl_pipe_properties *properties,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureGetPipeInfo_param_value(bool isCallValid,
                                    cl_mem pipe,
                                    PipeInfo param_namePacked,
                                    size_t param_value_size,
                                    void *param_value,
                                    size_t *param_value_size_ret,
                                    angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetPipeInfo_param_value_size_ret(bool isCallValid,
                                             cl_mem pipe,
                                             PipeInfo param_namePacked,
                                             size_t param_value_size,
                                             void *param_value,
                                             size_t *param_value_size_ret,
                                             angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureSVMFree_svm_pointer(bool isCallValid,
                                cl_context context,
                                void *svm_pointer,
                                angle::ParamCapture *paramCapture)
{
    // Nothing to implement. svm_pointer is an SVM pointer
}
void CaptureCreateSamplerWithProperties_sampler_properties(
    bool isCallValid,
    cl_context context,
    const cl_sampler_properties *sampler_properties,
    cl_int *errcode_ret,
    angle::ParamCapture *paramCapture)
{
    if (sampler_properties)
    {
        size_t propertiesSize = 0;
        while (sampler_properties[propertiesSize++] != 0)
        {
        }
        CaptureMemory((void *)sampler_properties, sizeof(cl_sampler_properties) * propertiesSize,
                      paramCapture);
    }
}
void CaptureCreateSamplerWithProperties_errcode_ret(bool isCallValid,
                                                    cl_context context,
                                                    const cl_sampler_properties *sampler_properties,
                                                    cl_int *errcode_ret,
                                                    angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureSetKernelArgSVMPointer_arg_value(bool isCallValid,
                                             cl_kernel kernel,
                                             cl_uint arg_index,
                                             const void *arg_value,
                                             angle::ParamCapture *paramCapture)
{
    // Nothing to implement. arg_value is an SVM pointer
}
void CaptureSetKernelExecInfo_param_value(bool isCallValid,
                                          cl_kernel kernel,
                                          KernelExecInfo param_namePacked,
                                          size_t param_value_size,
                                          const void *param_value,
                                          angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureEnqueueSVMFree_svm_pointers(bool isCallValid,
                                        cl_command_queue command_queue,
                                        cl_uint num_svm_pointers,
                                        void *svm_pointers[],
                                        void(CL_CALLBACK *pfn_free_func)(cl_command_queue queue,
                                                                         cl_uint num_svm_pointers,
                                                                         void *svm_pointers[],
                                                                         void *user_data),
                                        void *user_data,
                                        cl_uint num_events_in_wait_list,
                                        const cl_event *event_wait_list,
                                        cl_event *event,
                                        angle::ParamCapture *paramCapture)
{
    if (svm_pointers)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLVoidVectorIndex(
            const_cast<const void **>(svm_pointers), num_svm_pointers, paramCapture);
    }
}
void CaptureEnqueueSVMFree_pfn_free_func(bool isCallValid,
                                         cl_command_queue command_queue,
                                         cl_uint num_svm_pointers,
                                         void *svm_pointers[],
                                         void(CL_CALLBACK *pfn_free_func)(cl_command_queue queue,
                                                                          cl_uint num_svm_pointers,
                                                                          void *svm_pointers[],
                                                                          void *user_data),
                                         void *user_data,
                                         cl_uint num_events_in_wait_list,
                                         const cl_event *event_wait_list,
                                         cl_event *event,
                                         angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureEnqueueSVMFree_user_data(bool isCallValid,
                                     cl_command_queue command_queue,
                                     cl_uint num_svm_pointers,
                                     void *svm_pointers[],
                                     void(CL_CALLBACK *pfn_free_func)(cl_command_queue queue,
                                                                      cl_uint num_svm_pointers,
                                                                      void *svm_pointers[],
                                                                      void *user_data),
                                     void *user_data,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureEnqueueSVMFree_event_wait_list(
    bool isCallValid,
    cl_command_queue command_queue,
    cl_uint num_svm_pointers,
    void *svm_pointers[],
    void(CL_CALLBACK *pfn_free_func)(cl_command_queue queue,
                                     cl_uint num_svm_pointers,
                                     void *svm_pointers[],
                                     void *user_data),
    void *user_data,
    cl_uint num_events_in_wait_list,
    const cl_event *event_wait_list,
    cl_event *event,
    angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueSVMFree_event(bool isCallValid,
                                 cl_command_queue command_queue,
                                 cl_uint num_svm_pointers,
                                 void *svm_pointers[],
                                 void(CL_CALLBACK *pfn_free_func)(cl_command_queue queue,
                                                                  cl_uint num_svm_pointers,
                                                                  void *svm_pointers[],
                                                                  void *user_data),
                                 void *user_data,
                                 cl_uint num_events_in_wait_list,
                                 const cl_event *event_wait_list,
                                 cl_event *event,
                                 angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueSVMMemcpy_dst_ptr(bool isCallValid,
                                     cl_command_queue command_queue,
                                     cl_bool blocking_copy,
                                     void *dst_ptr,
                                     const void *src_ptr,
                                     size_t size,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{
    if (src_ptr &&
        cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(src_ptr) == SIZE_MAX)
    {
        paramCapture->readBufferSizeBytes = size;
    }
}
void CaptureEnqueueSVMMemcpy_src_ptr(bool isCallValid,
                                     cl_command_queue command_queue,
                                     cl_bool blocking_copy,
                                     void *dst_ptr,
                                     const void *src_ptr,
                                     size_t size,
                                     cl_uint num_events_in_wait_list,
                                     const cl_event *event_wait_list,
                                     cl_event *event,
                                     angle::ParamCapture *paramCapture)
{
    if (src_ptr &&
        cl::Platform::GetDefault()->getFrameCaptureShared()->getCLVoidIndex(src_ptr) == SIZE_MAX)
    {
        CaptureMemory(src_ptr, size, paramCapture);
    }
}
void CaptureEnqueueSVMMemcpy_event_wait_list(bool isCallValid,
                                             cl_command_queue command_queue,
                                             cl_bool blocking_copy,
                                             void *dst_ptr,
                                             const void *src_ptr,
                                             size_t size,
                                             cl_uint num_events_in_wait_list,
                                             const cl_event *event_wait_list,
                                             cl_event *event,
                                             angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueSVMMemcpy_event(bool isCallValid,
                                   cl_command_queue command_queue,
                                   cl_bool blocking_copy,
                                   void *dst_ptr,
                                   const void *src_ptr,
                                   size_t size,
                                   cl_uint num_events_in_wait_list,
                                   const cl_event *event_wait_list,
                                   cl_event *event,
                                   angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueSVMMemFill_svm_ptr(bool isCallValid,
                                      cl_command_queue command_queue,
                                      void *svm_ptr,
                                      const void *pattern,
                                      size_t pattern_size,
                                      size_t size,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list,
                                      cl_event *event,
                                      angle::ParamCapture *paramCapture)
{
    // Nothing to implement. svm_ptr is an SVM pointer
}
void CaptureEnqueueSVMMemFill_pattern(bool isCallValid,
                                      cl_command_queue command_queue,
                                      void *svm_ptr,
                                      const void *pattern,
                                      size_t pattern_size,
                                      size_t size,
                                      cl_uint num_events_in_wait_list,
                                      const cl_event *event_wait_list,
                                      cl_event *event,
                                      angle::ParamCapture *paramCapture)
{
    if (pattern)
    {
        CaptureMemory(pattern, pattern_size, paramCapture);
    }
}
void CaptureEnqueueSVMMemFill_event_wait_list(bool isCallValid,
                                              cl_command_queue command_queue,
                                              void *svm_ptr,
                                              const void *pattern,
                                              size_t pattern_size,
                                              size_t size,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueSVMMemFill_event(bool isCallValid,
                                    cl_command_queue command_queue,
                                    void *svm_ptr,
                                    const void *pattern,
                                    size_t pattern_size,
                                    size_t size,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueSVMMap_svm_ptr(bool isCallValid,
                                  cl_command_queue command_queue,
                                  cl_bool blocking_map,
                                  MapFlags flagsPacked,
                                  void *svm_ptr,
                                  size_t size,
                                  cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list,
                                  cl_event *event,
                                  angle::ParamCapture *paramCapture)
{
    // Nothing to implement. svm_ptr is an SVM pointer
}
void CaptureEnqueueSVMMap_event_wait_list(bool isCallValid,
                                          cl_command_queue command_queue,
                                          cl_bool blocking_map,
                                          MapFlags flagsPacked,
                                          void *svm_ptr,
                                          size_t size,
                                          cl_uint num_events_in_wait_list,
                                          const cl_event *event_wait_list,
                                          cl_event *event,
                                          angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueSVMMap_event(bool isCallValid,
                                cl_command_queue command_queue,
                                cl_bool blocking_map,
                                MapFlags flagsPacked,
                                void *svm_ptr,
                                size_t size,
                                cl_uint num_events_in_wait_list,
                                const cl_event *event_wait_list,
                                cl_event *event,
                                angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureEnqueueSVMUnmap_svm_ptr(bool isCallValid,
                                    cl_command_queue command_queue,
                                    void *svm_ptr,
                                    cl_uint num_events_in_wait_list,
                                    const cl_event *event_wait_list,
                                    cl_event *event,
                                    angle::ParamCapture *paramCapture)
{
    // Nothing to implement. svm_ptr is an SVM pointer
}
void CaptureEnqueueSVMUnmap_event_wait_list(bool isCallValid,
                                            cl_command_queue command_queue,
                                            void *svm_ptr,
                                            cl_uint num_events_in_wait_list,
                                            const cl_event *event_wait_list,
                                            cl_event *event,
                                            angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueSVMUnmap_event(bool isCallValid,
                                  cl_command_queue command_queue,
                                  void *svm_ptr,
                                  cl_uint num_events_in_wait_list,
                                  const cl_event *event_wait_list,
                                  cl_event *event,
                                  angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureGetDeviceAndHostTimer_device_timestamp(bool isCallValid,
                                                   cl_device_id device,
                                                   cl_ulong *device_timestamp,
                                                   cl_ulong *host_timestamp,
                                                   angle::ParamCapture *paramCapture)
{
    if (device_timestamp)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_ulong);
    }
}
void CaptureGetDeviceAndHostTimer_host_timestamp(bool isCallValid,
                                                 cl_device_id device,
                                                 cl_ulong *device_timestamp,
                                                 cl_ulong *host_timestamp,
                                                 angle::ParamCapture *paramCapture)
{
    if (host_timestamp)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_ulong);
    }
}
void CaptureGetHostTimer_host_timestamp(bool isCallValid,
                                        cl_device_id device,
                                        cl_ulong *host_timestamp,
                                        angle::ParamCapture *paramCapture)
{
    if (host_timestamp)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_ulong);
    }
}
void CaptureCreateProgramWithIL_il(bool isCallValid,
                                   cl_context context,
                                   const void *il,
                                   size_t length,
                                   cl_int *errcode_ret,
                                   angle::ParamCapture *paramCapture)
{
    if (il)
    {
        CaptureMemory(il, length, paramCapture);
    }
}
void CaptureCreateProgramWithIL_errcode_ret(bool isCallValid,
                                            cl_context context,
                                            const void *il,
                                            size_t length,
                                            cl_int *errcode_ret,
                                            angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCloneKernel_errcode_ret(bool isCallValid,
                                    cl_kernel source_kernel,
                                    cl_int *errcode_ret,
                                    angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureGetKernelSubGroupInfo_input_value(bool isCallValid,
                                              cl_kernel kernel,
                                              cl_device_id device,
                                              KernelSubGroupInfo param_namePacked,
                                              size_t input_value_size,
                                              const void *input_value,
                                              size_t param_value_size,
                                              void *param_value,
                                              size_t *param_value_size_ret,
                                              angle::ParamCapture *paramCapture)
{
    if (input_value)
    {
        CaptureMemory(input_value, input_value_size, paramCapture);
    }
}
void CaptureGetKernelSubGroupInfo_param_value(bool isCallValid,
                                              cl_kernel kernel,
                                              cl_device_id device,
                                              KernelSubGroupInfo param_namePacked,
                                              size_t input_value_size,
                                              const void *input_value,
                                              size_t param_value_size,
                                              void *param_value,
                                              size_t *param_value_size_ret,
                                              angle::ParamCapture *paramCapture)
{
    if (param_value)
    {
        paramCapture->readBufferSizeBytes = param_value_size;
    }
}
void CaptureGetKernelSubGroupInfo_param_value_size_ret(bool isCallValid,
                                                       cl_kernel kernel,
                                                       cl_device_id device,
                                                       KernelSubGroupInfo param_namePacked,
                                                       size_t input_value_size,
                                                       const void *input_value,
                                                       size_t param_value_size,
                                                       void *param_value,
                                                       size_t *param_value_size_ret,
                                                       angle::ParamCapture *paramCapture)
{
    if (param_value_size_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(size_t);
    }
}
void CaptureEnqueueSVMMigrateMem_svm_pointers(bool isCallValid,
                                              cl_command_queue command_queue,
                                              cl_uint num_svm_pointers,
                                              const void **svm_pointers,
                                              const size_t *sizes,
                                              MemMigrationFlags flagsPacked,
                                              cl_uint num_events_in_wait_list,
                                              const cl_event *event_wait_list,
                                              cl_event *event,
                                              angle::ParamCapture *paramCapture)
{
    if (svm_pointers)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLVoidVectorIndex(
            svm_pointers, num_svm_pointers, paramCapture);
    }
}
void CaptureEnqueueSVMMigrateMem_sizes(bool isCallValid,
                                       cl_command_queue command_queue,
                                       cl_uint num_svm_pointers,
                                       const void **svm_pointers,
                                       const size_t *sizes,
                                       MemMigrationFlags flagsPacked,
                                       cl_uint num_events_in_wait_list,
                                       const cl_event *event_wait_list,
                                       cl_event *event,
                                       angle::ParamCapture *paramCapture)
{
    if (sizes)
    {
        CaptureMemory(sizes, num_svm_pointers * sizeof(size_t), paramCapture);
    }
}
void CaptureEnqueueSVMMigrateMem_event_wait_list(bool isCallValid,
                                                 cl_command_queue command_queue,
                                                 cl_uint num_svm_pointers,
                                                 const void **svm_pointers,
                                                 const size_t *sizes,
                                                 MemMigrationFlags flagsPacked,
                                                 cl_uint num_events_in_wait_list,
                                                 const cl_event *event_wait_list,
                                                 cl_event *event,
                                                 angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}
void CaptureEnqueueSVMMigrateMem_event(bool isCallValid,
                                       cl_command_queue command_queue,
                                       cl_uint num_svm_pointers,
                                       const void **svm_pointers,
                                       const size_t *sizes,
                                       MemMigrationFlags flagsPacked,
                                       cl_uint num_events_in_wait_list,
                                       const cl_event *event_wait_list,
                                       cl_event *event,
                                       angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}
void CaptureSetProgramReleaseCallback_pfn_notify(bool isCallValid,
                                                 cl_program program,
                                                 void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                               void *user_data),
                                                 void *user_data,
                                                 angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureSetProgramReleaseCallback_user_data(bool isCallValid,
                                                cl_program program,
                                                void(CL_CALLBACK *pfn_notify)(cl_program program,
                                                                              void *user_data),
                                                void *user_data,
                                                angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureSetProgramSpecializationConstant_spec_value(bool isCallValid,
                                                        cl_program program,
                                                        cl_uint spec_id,
                                                        size_t spec_size,
                                                        const void *spec_value,
                                                        angle::ParamCapture *paramCapture)
{
    if (spec_value)
    {
        CaptureMemory(spec_value, spec_size, paramCapture);
    }
}
void CaptureSetContextDestructorCallback_pfn_notify(
    bool isCallValid,
    cl_context context,
    void(CL_CALLBACK *pfn_notify)(cl_context context, void *user_data),
    void *user_data,
    angle::ParamCapture *paramCapture)
{
    // Nothing to implement
}
void CaptureSetContextDestructorCallback_user_data(bool isCallValid,
                                                   cl_context context,
                                                   void(CL_CALLBACK *pfn_notify)(cl_context context,
                                                                                 void *user_data),
                                                   void *user_data,
                                                   angle::ParamCapture *paramCapture)
{
    InitParamValue(angle::ParamType::TvoidPointer, static_cast<void *>(nullptr),
                   &paramCapture->value);
}
void CaptureCreateBufferWithProperties_properties(bool isCallValid,
                                                  cl_context context,
                                                  const cl_mem_properties *properties,
                                                  MemFlags flagsPacked,
                                                  size_t size,
                                                  void *host_ptr,
                                                  cl_int *errcode_ret,
                                                  angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        size_t propertiesSize = 0;
        while (properties[propertiesSize++] != 0)
        {
        }
        CaptureMemory(properties, propertiesSize, paramCapture);
    }
}
void CaptureCreateBufferWithProperties_host_ptr(bool isCallValid,
                                                cl_context context,
                                                const cl_mem_properties *properties,
                                                MemFlags flagsPacked,
                                                size_t size,
                                                void *host_ptr,
                                                cl_int *errcode_ret,
                                                angle::ParamCapture *paramCapture)
{
    if (host_ptr)
    {
        CaptureMemory(host_ptr, size, paramCapture);
    }
}
void CaptureCreateBufferWithProperties_errcode_ret(bool isCallValid,
                                                   cl_context context,
                                                   const cl_mem_properties *properties,
                                                   MemFlags flagsPacked,
                                                   size_t size,
                                                   void *host_ptr,
                                                   cl_int *errcode_ret,
                                                   angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}
void CaptureCreateImageWithProperties_properties(bool isCallValid,
                                                 cl_context context,
                                                 const cl_mem_properties *properties,
                                                 MemFlags flagsPacked,
                                                 const cl_image_format *image_format,
                                                 const cl_image_desc *image_desc,
                                                 void *host_ptr,
                                                 cl_int *errcode_ret,
                                                 angle::ParamCapture *paramCapture)
{
    if (properties)
    {
        CaptureMemory(properties, sizeof(cl_mem_properties), paramCapture);
    }
}
void CaptureCreateImageWithProperties_image_format(bool isCallValid,
                                                   cl_context context,
                                                   const cl_mem_properties *properties,
                                                   MemFlags flagsPacked,
                                                   const cl_image_format *image_format,
                                                   const cl_image_desc *image_desc,
                                                   void *host_ptr,
                                                   cl_int *errcode_ret,
                                                   angle::ParamCapture *paramCapture)
{
    if (image_format)
    {
        CaptureMemory(image_format, sizeof(cl_image_format), paramCapture);
    }
}
void CaptureCreateImageWithProperties_image_desc(bool isCallValid,
                                                 cl_context context,
                                                 const cl_mem_properties *properties,
                                                 MemFlags flagsPacked,
                                                 const cl_image_format *image_format,
                                                 const cl_image_desc *image_desc,
                                                 void *host_ptr,
                                                 cl_int *errcode_ret,
                                                 angle::ParamCapture *paramCapture)
{
    if (image_desc)
    {
        CaptureMemory(image_desc, sizeof(cl_image_desc), paramCapture);
    }
}
void CaptureCreateImageWithProperties_host_ptr(bool isCallValid,
                                               cl_context context,
                                               const cl_mem_properties *properties,
                                               MemFlags flagsPacked,
                                               const cl_image_format *image_format,
                                               const cl_image_desc *image_desc,
                                               void *host_ptr,
                                               cl_int *errcode_ret,
                                               angle::ParamCapture *paramCapture)
{
    if (host_ptr && image_desc)
    {
        size_t image_size = 0;
        switch (image_desc->image_type)
        {
            case CL_MEM_OBJECT_IMAGE1D:
            case CL_MEM_OBJECT_IMAGE1D_BUFFER:
                image_size = image_desc->image_row_pitch;
                break;
            case CL_MEM_OBJECT_IMAGE2D:
                image_size = image_desc->image_row_pitch * image_desc->image_height;
                break;
            case CL_MEM_OBJECT_IMAGE3D:
                image_size = image_desc->image_slice_pitch * image_desc->image_depth;
                break;
            case CL_MEM_OBJECT_IMAGE1D_ARRAY:
            case CL_MEM_OBJECT_IMAGE2D_ARRAY:
                image_size = image_desc->image_slice_pitch * image_desc->image_array_size;
                break;
            default:
                break;
        }

        // Add a buffer
        image_size += 16;

        CaptureMemory(host_ptr, sizeof(image_size), paramCapture);
    }
}
void CaptureCreateImageWithProperties_errcode_ret(bool isCallValid,
                                                  cl_context context,
                                                  const cl_mem_properties *properties,
                                                  MemFlags flagsPacked,
                                                  const cl_image_format *image_format,
                                                  const cl_image_desc *image_desc,
                                                  void *host_ptr,
                                                  cl_int *errcode_ret,
                                                  angle::ParamCapture *paramCapture)
{
    if (errcode_ret)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_int);
    }
}

void CaptureEnqueueAcquireExternalMemObjectsKHR_mem_objects(bool isCallValid,
                                                            cl_command_queue command_queue,
                                                            cl_uint num_mem_objects,
                                                            const cl_mem *mem_objects,
                                                            cl_uint num_events_in_wait_list,
                                                            const cl_event *event_wait_list,
                                                            cl_event *event,
                                                            angle::ParamCapture *paramCapture)
{
    if (mem_objects)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            mem_objects, num_mem_objects, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}

void CaptureEnqueueAcquireExternalMemObjectsKHR_event_wait_list(bool isCallValid,
                                                                cl_command_queue command_queue,
                                                                cl_uint num_mem_objects,
                                                                const cl_mem *mem_objects,
                                                                cl_uint num_events_in_wait_list,
                                                                const cl_event *event_wait_list,
                                                                cl_event *event,
                                                                angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}

void CaptureEnqueueAcquireExternalMemObjectsKHR_event(bool isCallValid,
                                                      cl_command_queue command_queue,
                                                      cl_uint num_mem_objects,
                                                      const cl_mem *mem_objects,
                                                      cl_uint num_events_in_wait_list,
                                                      const cl_event *event_wait_list,
                                                      cl_event *event,
                                                      angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}

void CaptureEnqueueReleaseExternalMemObjectsKHR_mem_objects(bool isCallValid,
                                                            cl_command_queue command_queue,
                                                            cl_uint num_mem_objects,
                                                            const cl_mem *mem_objects,
                                                            cl_uint num_events_in_wait_list,
                                                            const cl_event *event_wait_list,
                                                            cl_event *event,
                                                            angle::ParamCapture *paramCapture)
{
    if (mem_objects)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            mem_objects, num_mem_objects, paramCapture, &angle::FrameCaptureShared::getIndex);
    }
}

void CaptureEnqueueReleaseExternalMemObjectsKHR_event_wait_list(bool isCallValid,
                                                                cl_command_queue command_queue,
                                                                cl_uint num_mem_objects,
                                                                const cl_mem *mem_objects,
                                                                cl_uint num_events_in_wait_list,
                                                                const cl_event *event_wait_list,
                                                                cl_event *event,
                                                                angle::ParamCapture *paramCapture)
{
    if (event_wait_list)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLObjVectorMap(
            event_wait_list, num_events_in_wait_list, paramCapture,
            &angle::FrameCaptureShared::getIndex);
    }
}

void CaptureEnqueueReleaseExternalMemObjectsKHR_event(bool isCallValid,
                                                      cl_command_queue command_queue,
                                                      cl_uint num_mem_objects,
                                                      const cl_mem *mem_objects,
                                                      cl_uint num_events_in_wait_list,
                                                      const cl_event *event_wait_list,
                                                      cl_event *event,
                                                      angle::ParamCapture *paramCapture)
{
    if (event)
    {
        InitParamValue(angle::ParamType::Tcl_event, *event, &paramCapture->value);
        cl::Platform::GetDefault()->getFrameCaptureShared()->setIndex(event);
    }
}

void CaptureIcdGetPlatformIDsKHR_platforms(bool isCallValid,
                                           cl_uint num_entries,
                                           cl_platform_id *platforms,
                                           cl_uint *num_platforms,
                                           angle::ParamCapture *paramCapture)
{
    if (platforms)
    {
        cl::Platform::GetDefault()->getFrameCaptureShared()->setCLPlatformIndices(platforms,
                                                                                  num_entries);
    }
}
void CaptureIcdGetPlatformIDsKHR_num_platforms(bool isCallValid,
                                               cl_uint num_entries,
                                               cl_platform_id *platforms,
                                               cl_uint *num_platforms,
                                               angle::ParamCapture *paramCapture)
{
    if (num_platforms)
    {
        paramCapture->readBufferSizeBytes = sizeof(cl_uint);
    }
}
}  // namespace cl
