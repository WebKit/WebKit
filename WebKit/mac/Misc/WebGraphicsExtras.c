/*
 * Copyright (C) 2006 Apple Computer, Inc.  All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Computer, Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "WebGraphicsExtras.h"

#import <Accelerate/Accelerate.h>
#import <dlfcn.h>
#import <wtf/Assertions.h>

unsigned WebConvertBGRAToARGB(unsigned char *offscreenBuffer, int rowBytes, int x, int y, int width, int height)
{
    
    static vImage_Error (*softLink_vImagePermuteChannels_ARGB8888)(const vImage_Buffer *src, const vImage_Buffer *dest, const uint8_t permuteMap[4], vImage_Flags flags) = NULL;

    if (!softLink_vImagePermuteChannels_ARGB8888) {
        void *framework = dlopen("/System/Library/Frameworks/Accelerate.framework/Accelerate", RTLD_NOW);
        ASSERT(framework);
        softLink_vImagePermuteChannels_ARGB8888 = dlsym(framework, "vImagePermuteChannels_ARGB8888");
        ASSERT(softLink_vImagePermuteChannels_ARGB8888);
    }
        
    void *swizzleImageBase = offscreenBuffer + y * rowBytes + x * 4;
    vImage_Buffer vImage = { swizzleImageBase, height, width, rowBytes };
    uint8_t vImagePermuteMap[4] = { 3, 2, 1, 0 }; // Where { 0, 1, 2, 3 } would leave the channels unchanged; this map converts BGRA to ARGB
    vImage_Error vImageError = softLink_vImagePermuteChannels_ARGB8888(&vImage, &vImage, vImagePermuteMap, 0);
    if (vImageError) {
        LOG_ERROR("Could not convert BGRA image to ARGB: %zd", vImageError);
        return FALSE;
    }
    
    return TRUE;
}
