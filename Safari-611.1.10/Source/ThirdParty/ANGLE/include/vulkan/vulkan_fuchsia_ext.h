//
// Copyright 2019 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// vulkan_fuchsia_ext:
//    Defines Fuchsia-specific Vulkan extensions that haven't been
//    upstreamed to the official Vulkan headers.
//

#ifndef COMMON_VULKAN_FUCHSIA_EXT_H_
#define COMMON_VULKAN_FUCHSIA_EXT_H_

#if !defined(VK_NO_PROTOTYPES)
#    define VK_NO_PROTOTYPES
#endif

#include <vulkan/vulkan.h>

#if defined(ANGLE_PLATFORM_FUCHSIA)
#    include <zircon/types.h>
#else
typedef uint32_t zx_handle_t;
#    define ZX_HANDLE_INVALID ((zx_handle_t)0)
#endif

#ifdef __cplusplus
extern "C" {
#endif

const VkStructureType VK_STRUCTURE_TYPE_TEMP_IMPORT_MEMORY_ZIRCON_HANDLE_INFO_FUCHSIA =
    static_cast<VkStructureType>(1001005000);
const VkStructureType VK_STRUCTURE_TYPE_TEMP_MEMORY_ZIRCON_HANDLE_PROPERTIES_FUCHSIA =
    static_cast<VkStructureType>(1001005001);
const VkStructureType VK_STRUCTURE_TYPE_TEMP_MEMORY_GET_ZIRCON_HANDLE_INFO_FUCHSIA =
    static_cast<VkStructureType>(1001005002);
const VkStructureType VK_STRUCTURE_TYPE_TEMP_IMPORT_SEMAPHORE_ZIRCON_HANDLE_INFO_FUCHSIA =
    static_cast<VkStructureType>(1001006000);
const VkStructureType VK_STRUCTURE_TYPE_TEMP_SEMAPHORE_GET_ZIRCON_HANDLE_INFO_FUCHSIA =
    static_cast<VkStructureType>(1001006001);

const VkExternalMemoryHandleTypeFlagBits
    VK_EXTERNAL_MEMORY_HANDLE_TYPE_TEMP_ZIRCON_VMO_BIT_FUCHSIA =
        static_cast<VkExternalMemoryHandleTypeFlagBits>(0x00100000);

const VkExternalSemaphoreHandleTypeFlagBits
    VK_EXTERNAL_SEMAPHORE_HANDLE_TYPE_TEMP_ZIRCON_EVENT_BIT_FUCHSIA =
        static_cast<VkExternalSemaphoreHandleTypeFlagBits>(0x00100000);

#define VK_FUCHSIA_external_memory 1
#define VK_FUCHSIA_EXTERNAL_MEMORY_SPEC_VERSION 1
#define VK_FUCHSIA_EXTERNAL_MEMORY_EXTENSION_NAME "VK_FUCHSIA_external_memory"

typedef struct VkImportMemoryZirconHandleInfoFUCHSIA
{
    VkStructureType sType;
    const void *pNext;
    VkExternalMemoryHandleTypeFlagBits handleType;
    zx_handle_t handle;
} VkImportMemoryZirconHandleInfoFUCHSIA;

typedef struct VkMemoryZirconHandlePropertiesFUCHSIA
{
    VkStructureType sType;
    void *pNext;
    zx_handle_t memoryTypeBits;
} VkMemoryZirconHandlePropertiesFUCHSIA;

typedef struct VkMemoryGetZirconHandleInfoFUCHSIA
{
    VkStructureType sType;
    const void *pNext;
    VkDeviceMemory memory;
    VkExternalMemoryHandleTypeFlagBits handleType;
} VkMemoryGetZirconHandleInfoFUCHSIA;

typedef VkResult(VKAPI_PTR *PFN_vkGetMemoryZirconHandleFUCHSIA)(
    VkDevice device,
    const VkMemoryGetZirconHandleInfoFUCHSIA *pGetZirconHandleInfo,
    zx_handle_t *pZirconHandle);
typedef VkResult(VKAPI_PTR *PFN_vkGetMemoryZirconHandlePropertiesFUCHSIA)(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    zx_handle_t ZirconHandle,
    VkMemoryZirconHandlePropertiesFUCHSIA *pMemoryZirconHandleProperties);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL
vkGetMemoryZirconHandleFUCHSIA(VkDevice device,
                               const VkMemoryGetZirconHandleInfoFUCHSIA *pGetZirconHandleInfo,
                               zx_handle_t *pZirconHandle);

VKAPI_ATTR VkResult VKAPI_CALL vkGetMemoryZirconHandlePropertiesFUCHSIA(
    VkDevice device,
    VkExternalMemoryHandleTypeFlagBits handleType,
    zx_handle_t ZirconHandle,
    VkMemoryZirconHandlePropertiesFUCHSIA *pMemoryZirconHandleProperties);
#endif

#define VK_FUCHSIA_external_semaphore 1
#define VK_FUCHSIA_EXTERNAL_SEMAPHORE_SPEC_VERSION 1
#define VK_FUCHSIA_EXTERNAL_SEMAPHORE_EXTENSION_NAME "VK_FUCHSIA_external_semaphore"

typedef struct VkImportSemaphoreZirconHandleInfoFUCHSIA
{
    VkStructureType sType;
    const void *pNext;
    VkSemaphore semaphore;
    VkSemaphoreImportFlags flags;
    VkExternalSemaphoreHandleTypeFlagBits handleType;
    zx_handle_t handle;
} VkImportSemaphoreZirconHandleInfoFUCHSIA;

typedef struct VkSemaphoreGetZirconHandleInfoFUCHSIA
{
    VkStructureType sType;
    const void *pNext;
    VkSemaphore semaphore;
    VkExternalSemaphoreHandleTypeFlagBits handleType;
} VkSemaphoreGetZirconHandleInfoFUCHSIA;

typedef VkResult(VKAPI_PTR *PFN_vkImportSemaphoreZirconHandleFUCHSIA)(
    VkDevice device,
    const VkImportSemaphoreZirconHandleInfoFUCHSIA *pImportSemaphoreZirconHandleInfo);
typedef VkResult(VKAPI_PTR *PFN_vkGetSemaphoreZirconHandleFUCHSIA)(
    VkDevice device,
    const VkSemaphoreGetZirconHandleInfoFUCHSIA *pGetZirconHandleInfo,
    zx_handle_t *pZirconHandle);

#ifndef VK_NO_PROTOTYPES
VKAPI_ATTR VkResult VKAPI_CALL vkImportSemaphoreZirconHandleFUCHSIA(
    VkDevice device,
    const VkImportSemaphoreZirconHandleInfoFUCHSIA *pImportSemaphoreZirconHandleInfo);

VKAPI_ATTR VkResult VKAPI_CALL
vkGetSemaphoreZirconHandleFUCHSIA(VkDevice device,
                                  const VkSemaphoreGetZirconHandleInfoFUCHSIA *pGetZirconHandleInfo,
                                  zx_handle_t *pZirconHandle);
#endif

#ifdef __cplusplus
}
#endif

#endif  // COMMON_VULKAN_FUCHSIA_EXT_H_
