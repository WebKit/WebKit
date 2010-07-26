/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
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

#include "DummyPlugin.h"

#include <WebCore/GraphicsContext.h>

using namespace WebCore;

namespace WebKit {

DummyPlugin::DummyPlugin()
{
}

bool DummyPlugin::initialize(PluginController*, const Parameters&)
{
    return true;
}
    
void DummyPlugin::destroy()
{
}
    
void DummyPlugin::paint(GraphicsContext* context, const IntRect& dirtyRect)
{
#if PLATFORM(MAC)
    CGContextRef cgContext = context->platformContext();
    CGContextSaveGState(cgContext);
    
    CGColorRef redColor = CGColorCreateGenericRGB(1, 0, 0, 1);
    CGContextSetFillColorWithColor(cgContext, redColor);
    CGContextFillRect(cgContext, dirtyRect);
    CGColorRelease(redColor);
#endif    
}
    
void DummyPlugin::geometryDidChange(const IntRect& frameRect, const IntRect& clipRect)
{
}

void DummyPlugin::frameDidFinishLoading(uint64_t requestID)
{
}

void DummyPlugin::frameDidFail(uint64_t requestID, bool wasCancelled)
{
}

PluginController* DummyPlugin::controller()
{
    return 0;
}

void DummyPlugin::didEvaluateJavaScript(uint64_t requestID, const WebCore::String& requestURLString, const WebCore::String& result)
{
}

void DummyPlugin::streamDidReceiveResponse(uint64_t streamID, const WebCore::KURL& responseURL, uint32_t streamLength, 
                                           uint32_t lastModifiedTime, const WebCore::String& mimeType, const WebCore::String& headers)
{
}

void DummyPlugin::streamDidReceiveData(uint64_t streamID, const char* bytes, int length)
{
}

void DummyPlugin::streamDidFinishLoading(uint64_t streamID)
{
}

void DummyPlugin::streamDidFail(uint64_t streamID, bool wasCancelled)
{
}

bool DummyPlugin::handleMouseEvent(const WebMouseEvent&)
{
    return false;
}

bool DummyPlugin::handleWheelEvent(const WebWheelEvent&)
{
    return false;
}

} // namespace WebKit
