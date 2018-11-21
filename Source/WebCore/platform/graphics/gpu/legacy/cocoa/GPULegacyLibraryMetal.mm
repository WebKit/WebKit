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
#import "GPULegacyLibrary.h"

#if ENABLE(WEBMETAL)

#import "GPULegacyDevice.h"
#import "Logging.h"
#import <Metal/Metal.h>
#import <wtf/Vector.h>
#import <wtf/text/WTFString.h>

namespace WebCore {

GPULegacyLibrary::GPULegacyLibrary(const GPULegacyDevice& device, const String& sourceCode)
{
    LOG(WebMetal, "GPULegacyLibrary::GPULegacyLibrary()");

    if (!device.metal())
        return;

    NSError *error = [NSError errorWithDomain:@"com.apple.WebKit.GPU" code:1 userInfo:nil];
    m_metal = adoptNS([device.metal() newLibraryWithSource:sourceCode options:nil error:&error]);
    if (!m_metal)
        LOG(WebMetal, "Compilation error: %s", [[error localizedDescription] UTF8String]);
}

String GPULegacyLibrary::label() const
{
    if (!m_metal)
        return emptyString();

    return [m_metal label];
}

void GPULegacyLibrary::setLabel(const String& label) const
{
    ASSERT(m_metal);
    [m_metal setLabel:label];
}

Vector<String> GPULegacyLibrary::functionNames() const
{
    auto array = [m_metal functionNames];
    Vector<String> vector;
    vector.reserveInitialCapacity(array.count);
    for (NSString *string in array)
        vector.uncheckedAppend(string);
    return vector;
}

} // namespace WebCore

#endif
