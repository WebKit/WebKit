/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <CoreFoundation/CoreFoundation.h>
#include <WebGPU/WebGPU.h>
#include <dispatch/dispatch.h>
#include <wtf/darwin/DispatchExtras.h>

#define UNUSED_PARAM(variable) (void)variable

int main()
{
    WGPUInstanceCocoaDescriptor instanceCocoaDescriptor = {
        {
            NULL,
            (WGPUSType)WGPUSTypeExtended_InstanceCocoaDescriptor,
        },
        ^(WGPUWorkItem workItem)
        {
            dispatch_async(mainDispatchQueueSingleton(), workItem);
        },
    };
    WGPUInstanceDescriptor instanceDescriptor = {
        &instanceCocoaDescriptor.chain,
    };
    WGPUInstance instance = wgpuCreateInstance(&instanceDescriptor);

    WGPURequestAdapterOptions requestAdapterOptions = {
        NULL,
        NULL,
        WGPUPowerPreference_Undefined,
        false,
    };
    // FIXME: Update this when adapter creation is actually asynchronous
    __block WGPUAdapter adapter = NULL;
    wgpuInstanceRequestAdapterWithBlock(instance, &requestAdapterOptions, ^(WGPURequestAdapterStatus status, WGPUAdapter localAdapter, const char* message) {
        assert(status == WGPURequestAdapterStatus_Success);
        UNUSED_PARAM(message);
        adapter = localAdapter;
    });

    WGPUDeviceDescriptor deviceDescriptor = {
        NULL,
        NULL,
        0,
        NULL,
        NULL,
    };
    // FIXME: Update this when device creation is actually asynchronous
    __block WGPUDevice device = NULL;
    wgpuAdapterRequestDeviceWithBlock(adapter, &deviceDescriptor, ^(WGPURequestDeviceStatus status, WGPUDevice localDevice, const char* message) {
        assert(status == WGPURequestAdapterStatus_Success);
        UNUSED_PARAM(message);
        device = localDevice;
    });

    wgpuDevicePushErrorScope(device, WGPUErrorFilter_Validation);

    WGPUBufferDescriptor uploadBufferDescriptor = {
        NULL,
        NULL,
        WGPUBufferUsage_MapWrite | WGPUBufferUsage_CopySrc,
        sizeof(int32_t),
        false,
    };
    WGPUBuffer uploadBuffer = wgpuDeviceCreateBuffer(device, &uploadBufferDescriptor);

    WGPUBufferDescriptor downloadBufferDescriptor = {
        NULL,
        NULL,
        WGPUBufferUsage_MapRead | WGPUBufferUsage_CopyDst,
        sizeof(int32_t),
        false,
    };
    WGPUBuffer downloadBuffer = wgpuDeviceCreateBuffer(device, &downloadBufferDescriptor);

    wgpuBufferMapAsyncWithBlock(uploadBuffer, WGPUMapMode_Write, 0, sizeof(int32_t), ^(WGPUBufferMapAsyncStatus status) {
        assert(status == WGPUQueueWorkDoneStatus_Success);
        int32_t * writePointer = wgpuBufferGetMappedRange(uploadBuffer, 0, sizeof(int32_t));
        writePointer[0] = 17;
        wgpuBufferUnmap(uploadBuffer);

        WGPUCommandEncoderDescriptor commandEncoderDescriptor = {
            NULL,
            NULL,
        };
        WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDescriptor);
        wgpuCommandEncoderCopyBufferToBuffer(commandEncoder, uploadBuffer, 0, downloadBuffer, 0, sizeof(uint32_t));
        WGPUCommandBufferDescriptor commandBufferDescriptor = {
            NULL,
            NULL,
        };
        WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDescriptor);

        WGPUCommandBuffer commands[] = { commandBuffer };
        wgpuQueueSubmit(wgpuDeviceGetQueue(device), sizeof(commands) / sizeof(commands[0]), commands);

        wgpuQueueOnSubmittedWorkDoneWithBlock(wgpuDeviceGetQueue(device), 0, ^(WGPUQueueWorkDoneStatus status) {
            assert(status == WGPUQueueWorkDoneStatus_Success);
            wgpuBufferMapAsyncWithBlock(downloadBuffer, WGPUMapMode_Read, 0, sizeof(int32_t), ^(WGPUBufferMapAsyncStatus status) {
                assert(status == WGPUBufferMapAsyncStatus_Success);
                int32_t * readPointer = wgpuBufferGetMappedRange(downloadBuffer, 0, sizeof(int32_t));
                printf("Result: %" PRId32 "\n", readPointer[0]);
                wgpuBufferUnmap(downloadBuffer);
                wgpuDevicePopErrorScopeWithBlock(device, ^(WGPUErrorType type, const char* message) {
                    if (type != WGPUErrorType_NoError) {
                        if (message != nil)
                            printf("Message: %s\n", message);
                        else
                            printf("Empty message.\n");
                    } else
                        printf("Success!\n");

                    CFRunLoopStop(CFRunLoopGetMain());

                    wgpuCommandBufferRelease(commandBuffer);
                    wgpuCommandEncoderRelease(commandEncoder);
                });
            });
        });
    });

    CFRunLoopRun();

    wgpuBufferRelease(downloadBuffer);
    wgpuBufferRelease(uploadBuffer);
    wgpuDeviceRelease(device);
    wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
    return 0;
}
