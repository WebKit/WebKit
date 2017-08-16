/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#import <Foundation/Foundation.h>

#if USE(APPLE_INTERNAL_SDK)

// FIXME: Remove EAGL_IOSURFACE macro once header refactoring is completed.
#define EAGL_IOSURFACE 1
#import <OpenGLES/EAGLPrivate.h>
#undef EAGL_IOSURFACE

#else

typedef uint32_t EAGLContextParameter;

enum {
    kEAGLCPGPURestartStatusNone        = 0, /* context has not caused recent GPU restart */
    kEAGLCPGPURestartStatusCaused      = 1, /* context caused recent GPU restart (clear on query) */
    kEAGLCPGPURestartStatusBlacklisted = 2, /* context is being ignored for excessive GPU restarts */
};

/* (read-only) context caused GPU hang/crash, requiring a restart (see EAGLGPGPURestartStatus) */
#define kEAGLCPGPURestartStatus                   ((EAGLContextParameter)317)
/* (read-write) how to react to being blacklisted for causing excessive restarts (default to 1) */
#define kEAGLCPAbortOnGPURestartStatusBlacklisted ((EAGLContextParameter)318)
/* (read-only) does driver support auto-restart of GPU on hang/crash? */
#define kEAGLCPSupportGPURestart                  ((EAGLContextParameter)319)
/* (read-only) does driver/GPU support separate address space per context? */
#define kEAGLCPSupportSeparateAddressSpace        ((EAGLContextParameter)320)

@interface EAGLContext (EAGLPrivate)
- (NSUInteger)setParameter:(EAGLContextParameter)pname to:(int32_t *)params;
- (NSUInteger)getParameter:(EAGLContextParameter)pname to:(int32_t *)params;
@end
#endif
