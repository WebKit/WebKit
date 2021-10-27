/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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

#import "config.h"
#import "Test.h"

#if PLATFORM(COCOA) && ENABLE(WEBGL)
#import "WebCoreUtilities.h"
#import <Metal/Metal.h>
#import <WebCore/GraphicsContextGLOpenGL.h>

namespace TestWebKitAPI {
using namespace WebCore;

namespace {
class TestedGraphicsContextGLOpenGL : public GraphicsContextGLOpenGL {
public:
    static RefPtr<TestedGraphicsContextGLOpenGL> create(GraphicsContextGLAttributes attributes)
    {
        auto context = adoptRef(*new TestedGraphicsContextGLOpenGL(WTFMove(attributes)));
        return context;
    }
private:
    TestedGraphicsContextGLOpenGL(GraphicsContextGLAttributes attributes)
        : GraphicsContextGLOpenGL(WTFMove(attributes))
    {
    }
};
}

static bool hasMultipleGPUs()
{
#if (PLATFORM(MAC) || PLATFORM(MACCATALYST))
    auto devices = adoptNS(MTLCopyAllDevices());
    return [devices count] > 1;
#else
    return false;
#endif
}

#if HAVE(WEBGL_COMPATIBLE_METAL) && (PLATFORM(MAC) || PLATFORM(MACCATALYST))
#define MAYBE_MultipleGPUsDifferentPowerPreferenceMetal MultipleGPUsDifferentPowerPreferenceMetal
#else
#define MAYBE_MultipleGPUsDifferentPowerPreferenceMetal DISABLED_MultipleGPUsDifferentPowerPreferenceMetal
#endif
// Tests for a bug where high-performance context would use low-power GPU if low-power or default
// context was created first. Test is applicable only for Metal, since GPU selection for OpenGL is
// very different.
TEST(GraphicsContextGLOpenGLCocoaTest, MAYBE_MultipleGPUsDifferentPowerPreferenceMetal)
{
    if (!hasMultipleGPUs())
        return;
    ScopedSetAuxiliaryProcessTypeForTesting scopedProcessType { AuxiliaryProcessType::GPU };

    GraphicsContextGLAttributes attributes;
    attributes.useMetal = true;
    EXPECT_EQ(attributes.powerPreference, GraphicsContextGLPowerPreference::Default);
    auto defaultContext = TestedGraphicsContextGLOpenGL::create(attributes);
    EXPECT_NE(defaultContext, nullptr);

    attributes.powerPreference = GraphicsContextGLPowerPreference::LowPower;
    auto lowPowerContext = TestedGraphicsContextGLOpenGL::create(attributes);
    EXPECT_NE(lowPowerContext, nullptr);

    attributes.powerPreference = GraphicsContextGLPowerPreference::HighPerformance;
    auto highPerformanceContext = TestedGraphicsContextGLOpenGL::create(attributes);
    EXPECT_NE(highPerformanceContext, nullptr);

    EXPECT_NE(lowPowerContext->getString(GraphicsContextGL::RENDERER), highPerformanceContext->getString(GraphicsContextGL::RENDERER));
    EXPECT_EQ(defaultContext->getString(GraphicsContextGL::RENDERER), lowPowerContext->getString(GraphicsContextGL::RENDERER));
}

}
#endif
