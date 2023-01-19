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

#import "config.h"
#import "WebGPUCoordinator.h"

@import Foundation;
@import WebGPU;

#define UNUSED_PARAM(variable) (void)variable

@interface Scheduler: NSObject
- (instancetype)init;
- (void)schedule:(WGPUWorkItem)workItem;
@end

@implementation Scheduler {
    NSMutableArray<NSTimer *> *timers;
}

- (instancetype)init
{
    if (self = [super init])
        timers = [NSMutableArray new];
    return self;
}

- (void)schedule:(WGPUWorkItem)workItem
{
    [timers addObject:[NSTimer scheduledTimerWithTimeInterval:0 repeats:NO block:^(NSTimer *timer) {
        workItem();
        [self->timers removeObject:timer];
    }]];
}

@end

@implementation WebGPUCoordinator {
    Scheduler *scheduler;
    WGPUInstance instance;
    WGPUAdapter adapter;
    WGPUDevice device;
    WGPUSurface surface;
    WGPUSwapChain swapChain;
    MTKView *view;
}

- (instancetype)init
{
    if (self = [super init]) {
        scheduler = [Scheduler new];
        WGPUInstanceCocoaDescriptor instanceCocoaDescriptor = {
            {
                NULL,
                (WGPUSType)WGPUSTypeExtended_InstanceCocoaDescriptor,
            },
            ^(WGPUWorkItem workItem) {
                [self->scheduler schedule:workItem];
            },
        };
        WGPUInstanceDescriptor instanceDescriptor = {
            &instanceCocoaDescriptor.chain,
        };
        instance = wgpuCreateInstance(&instanceDescriptor);


        WGPURequestAdapterOptions requestAdapterOptions = {
            NULL,
            NULL,
            WGPUPowerPreference_Undefined,
            false,
        };
        wgpuInstanceRequestAdapterWithBlock(instance, &requestAdapterOptions, ^(WGPURequestAdapterStatus status, WGPUAdapter localAdapter, const char* message) {
            assert(status == WGPURequestAdapterStatus_Success);
            UNUSED_PARAM(message);
            self->adapter = localAdapter;

            WGPUDeviceDescriptor deviceDescriptor = {
                NULL,
                NULL,
                0,
                NULL,
                NULL,
            };
            wgpuAdapterRequestDeviceWithBlock(self->adapter, &deviceDescriptor, ^(WGPURequestDeviceStatus status, WGPUDevice localDevice, const char* message) {
                assert(status == WGPURequestAdapterStatus_Success);
                UNUSED_PARAM(message);
                self->device = localDevice;

                if (self->view && self->view.delegate) {
                    // We lost the race
                    [self->view.delegate mtkView:self->view drawableSizeWillChange:self->view.drawableSize];
                }
            });
        });
    }
    return self;
}

- (void)dealloc
{
    if (swapChain != nil)
        wgpuSwapChainRelease(swapChain);
    if (surface != nil)
        wgpuSurfaceRelease(surface);
    if (device != nil)
        wgpuDeviceRelease(device);
    if (adapter != nil)
        wgpuAdapterRelease(adapter);
    wgpuInstanceRelease(instance);
}

- (void)setView:(MTKView *)view
{
    assert(self->view == nil);
    self->view = view;

    WGPUSurfaceDescriptorFromMetalLayer surfaceDescriptorFromMetalLayer = {
        {
            NULL,
            WGPUSType_SurfaceDescriptorFromMetalLayer,
        },
        (__bridge void*)view.layer,
    };
    WGPUSurfaceDescriptor surfaceDescriptor = {
        &surfaceDescriptorFromMetalLayer.chain,
        NULL,
    };
    surface = wgpuInstanceCreateSurface(instance, &surfaceDescriptor);
}

- (void)drawInMTKView:(nonnull MTKView *)view
{
    if (!device || !swapChain)
        return;

    WGPUCommandEncoderDescriptor commandEncoderDescriptor = {
        NULL,
        NULL,
    };
    WGPUCommandEncoder commandEncoder = wgpuDeviceCreateCommandEncoder(device, &commandEncoderDescriptor);

    WGPUTextureView textureView = wgpuSwapChainGetCurrentTextureView(swapChain);
    WGPURenderPassColorAttachment colorAttachments[] = { {
        textureView,
        NULL,
        WGPULoadOp_Clear,
        WGPUStoreOp_Store, {
            0,
            (sin(mach_absolute_time() / 2000000) + 1) / 2,
            0,
            1,
        },
    } };
    WGPURenderPassDescriptor renderPassDescriptor = {
        NULL,
        NULL,
        sizeof(colorAttachments) / sizeof(colorAttachments[0]),
        colorAttachments,
        NULL,
        NULL,
        0,
        NULL,
    };
    WGPURenderPassEncoder renderPassEncoder = wgpuCommandEncoderBeginRenderPass(commandEncoder, &renderPassDescriptor);

    wgpuRenderPassEncoderEndPass(renderPassEncoder);

    WGPUCommandBufferDescriptor commandBufferDescriptor = {
        NULL,
        NULL,
    };
    WGPUCommandBuffer commandBuffer = wgpuCommandEncoderFinish(commandEncoder, &commandBufferDescriptor);

    WGPUCommandBuffer commands[] = { commandBuffer };
    wgpuQueueSubmit(wgpuDeviceGetQueue(device), sizeof(commands) / sizeof(commands[0]), commands);

    wgpuSwapChainPresent(swapChain);

    wgpuCommandBufferRelease(commandBuffer);
    wgpuRenderPassEncoderRelease(renderPassEncoder);
    wgpuCommandEncoderRelease(commandEncoder);
}

- (void)mtkView:(nonnull MTKView *)view drawableSizeWillChange:(CGSize)size
{
    if (!device || !surface) {
        // The device is still being brought up. No biggie; this function will be called again when it's done.
        return;
    }

    if (swapChain != nil)
        wgpuSwapChainRelease(swapChain);

    WGPUSwapChainDescriptor swapChainDescriptor = {
        NULL,
        NULL,
        WGPUTextureUsage_RenderAttachment,
        WGPUTextureFormat_BGRA8Unorm,
        size.width,
        size.height,
        WGPUPresentMode_Immediate
    };
    swapChain = wgpuDeviceCreateSwapChain(device, surface, &swapChainDescriptor);
    view.device = ((CAMetalLayer *)view.layer).device;
}

@end
