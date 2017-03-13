/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
#import "GPULibrary.h"

#if ENABLE(WEBGPU)

#import "GPUDevice.h"
#import "Logging.h"

#import <Metal/Metal.h>

namespace WebCore {

GPULibrary::GPULibrary(GPUDevice* device, const String& sourceCode)
{
    LOG(WebGPU, "GPULibrary::GPULibrary()");

    if (!device || !device->platformDevice())
        return;

    NSError *error = [NSError errorWithDomain:@"com.apple.WebKit.GPU" code:1 userInfo:nil];
    m_library = adoptNS((MTLLibrary *)[device->platformDevice() newLibraryWithSource:sourceCode options:nil error:&error]);
    if (!m_library)
        LOG(WebGPU, "Compilation error: %s", [[error localizedDescription] UTF8String]);
}

String GPULibrary::label() const
{
    if (!m_library)
        return emptyString();

    return [m_library label];
}

void GPULibrary::setLabel(const String& label)
{
    ASSERT(m_library);
    [m_library setLabel:label];
}

Vector<String> GPULibrary::functionNames()
{
    if (!m_library)
        return Vector<String>();

    Vector<String> names;

    NSArray<NSString*> *functionNames = [m_library functionNames];
    for (NSString *string in functionNames)
        names.append(string);
    
    return names;
}

MTLLibrary *GPULibrary::platformLibrary()
{
    return m_library.get();
}

} // namespace WebCore

#endif
