/*******************************************************************************
 * Copyright (c) 2008-2023 The Khronos Group Inc.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *    http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 ******************************************************************************/

#ifndef OPENCL_CL_VA_API_MEDIA_SHARING_INTEL_H_
#define OPENCL_CL_VA_API_MEDIA_SHARING_INTEL_H_

/*
** This header is generated from the Khronos OpenCL XML API Registry.
*/

#include <va/va.h>

#include <CL/cl.h>

/* CL_NO_PROTOTYPES implies CL_NO_EXTENSION_PROTOTYPES: */
#if defined(CL_NO_PROTOTYPES) && !defined(CL_NO_EXTENSION_PROTOTYPES)
#define CL_NO_EXTENSION_PROTOTYPES
#endif

/* CL_NO_EXTENSION_PROTOTYPES implies
   CL_NO_ICD_DISPATCH_EXTENSION_PROTOTYPES and
   CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES: */
#if defined(CL_NO_EXTENSION_PROTOTYPES) && \
    !defined(CL_NO_ICD_DISPATCH_EXTENSION_PROTOTYPES)
#define CL_NO_ICD_DISPATCH_EXTENSION_PROTOTYPES
#endif
#if defined(CL_NO_EXTENSION_PROTOTYPES) && \
    !defined(CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES)
#define CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES
#endif

#ifdef __cplusplus
extern "C" {
#endif

/***************************************************************
* cl_intel_sharing_format_query_va_api
***************************************************************/
#define cl_intel_sharing_format_query_va_api 1
#define CL_INTEL_SHARING_FORMAT_QUERY_VA_API_EXTENSION_NAME \
    "cl_intel_sharing_format_query_va_api"

/* when cl_intel_va_api_media_sharing is supported */

typedef cl_int CL_API_CALL
clGetSupportedVA_APIMediaSurfaceFormatsINTEL_t(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type image_type,
    cl_uint plane,
    cl_uint num_entries,
    VAImageFormat* va_api_formats,
    cl_uint* num_surface_formats);

typedef clGetSupportedVA_APIMediaSurfaceFormatsINTEL_t *
clGetSupportedVA_APIMediaSurfaceFormatsINTEL_fn ;

#if !defined(CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES)

extern CL_API_ENTRY cl_int CL_API_CALL
clGetSupportedVA_APIMediaSurfaceFormatsINTEL(
    cl_context context,
    cl_mem_flags flags,
    cl_mem_object_type image_type,
    cl_uint plane,
    cl_uint num_entries,
    VAImageFormat* va_api_formats,
    cl_uint* num_surface_formats) ;

#endif /* !defined(CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES) */

/***************************************************************
* cl_intel_va_api_media_sharing
***************************************************************/
#define cl_intel_va_api_media_sharing 1
#define CL_INTEL_VA_API_MEDIA_SHARING_EXTENSION_NAME \
    "cl_intel_va_api_media_sharing"

typedef cl_uint             cl_va_api_device_source_intel;
typedef cl_uint             cl_va_api_device_set_intel;

/* Error codes */
#define CL_INVALID_VA_API_MEDIA_ADAPTER_INTEL               -1098
#define CL_INVALID_VA_API_MEDIA_SURFACE_INTEL               -1099
#define CL_VA_API_MEDIA_SURFACE_ALREADY_ACQUIRED_INTEL      -1100
#define CL_VA_API_MEDIA_SURFACE_NOT_ACQUIRED_INTEL          -1101

/* cl_va_api_device_source_intel */
#define CL_VA_API_DISPLAY_INTEL                             0x4094

/* cl_va_api_device_set_intel */
#define CL_PREFERRED_DEVICES_FOR_VA_API_INTEL               0x4095
#define CL_ALL_DEVICES_FOR_VA_API_INTEL                     0x4096

/* cl_context_info */
#define CL_CONTEXT_VA_API_DISPLAY_INTEL                     0x4097

/* cl_mem_info */
#define CL_MEM_VA_API_MEDIA_SURFACE_INTEL                   0x4098

/* cl_image_info */
#define CL_IMAGE_VA_API_PLANE_INTEL                         0x4099

/* cl_command_type */
#define CL_COMMAND_ACQUIRE_VA_API_MEDIA_SURFACES_INTEL      0x409A
#define CL_COMMAND_RELEASE_VA_API_MEDIA_SURFACES_INTEL      0x409B


typedef cl_int CL_API_CALL
clGetDeviceIDsFromVA_APIMediaAdapterINTEL_t(
    cl_platform_id platform,
    cl_va_api_device_source_intel media_adapter_type,
    void* media_adapter,
    cl_va_api_device_set_intel media_adapter_set,
    cl_uint num_entries,
    cl_device_id* devices,
    cl_uint* num_devices);

typedef clGetDeviceIDsFromVA_APIMediaAdapterINTEL_t *
clGetDeviceIDsFromVA_APIMediaAdapterINTEL_fn CL_API_SUFFIX__VERSION_1_2;

typedef cl_mem CL_API_CALL
clCreateFromVA_APIMediaSurfaceINTEL_t(
    cl_context context,
    cl_mem_flags flags,
    VASurfaceID* surface,
    cl_uint plane,
    cl_int* errcode_ret);

typedef clCreateFromVA_APIMediaSurfaceINTEL_t *
clCreateFromVA_APIMediaSurfaceINTEL_fn CL_API_SUFFIX__VERSION_1_2;

typedef cl_int CL_API_CALL
clEnqueueAcquireVA_APIMediaSurfacesINTEL_t(
    cl_command_queue command_queue,
    cl_uint num_objects,
    const cl_mem* mem_objects,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event);

typedef clEnqueueAcquireVA_APIMediaSurfacesINTEL_t *
clEnqueueAcquireVA_APIMediaSurfacesINTEL_fn CL_API_SUFFIX__VERSION_1_2;

typedef cl_int CL_API_CALL
clEnqueueReleaseVA_APIMediaSurfacesINTEL_t(
    cl_command_queue command_queue,
    cl_uint num_objects,
    const cl_mem* mem_objects,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event);

typedef clEnqueueReleaseVA_APIMediaSurfacesINTEL_t *
clEnqueueReleaseVA_APIMediaSurfacesINTEL_fn CL_API_SUFFIX__VERSION_1_2;

#if !defined(CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES)

extern CL_API_ENTRY cl_int CL_API_CALL
clGetDeviceIDsFromVA_APIMediaAdapterINTEL(
    cl_platform_id platform,
    cl_va_api_device_source_intel media_adapter_type,
    void* media_adapter,
    cl_va_api_device_set_intel media_adapter_set,
    cl_uint num_entries,
    cl_device_id* devices,
    cl_uint* num_devices) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_mem CL_API_CALL
clCreateFromVA_APIMediaSurfaceINTEL(
    cl_context context,
    cl_mem_flags flags,
    VASurfaceID* surface,
    cl_uint plane,
    cl_int* errcode_ret) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL
clEnqueueAcquireVA_APIMediaSurfacesINTEL(
    cl_command_queue command_queue,
    cl_uint num_objects,
    const cl_mem* mem_objects,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event) CL_API_SUFFIX__VERSION_1_2;

extern CL_API_ENTRY cl_int CL_API_CALL
clEnqueueReleaseVA_APIMediaSurfacesINTEL(
    cl_command_queue command_queue,
    cl_uint num_objects,
    const cl_mem* mem_objects,
    cl_uint num_events_in_wait_list,
    const cl_event* event_wait_list,
    cl_event* event) CL_API_SUFFIX__VERSION_1_2;

#endif /* !defined(CL_NO_NON_ICD_DISPATCH_EXTENSION_PROTOTYPES) */

#ifdef __cplusplus
}
#endif

#endif /* OPENCL_CL_VA_API_MEDIA_SHARING_INTEL_H_ */
