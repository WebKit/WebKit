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

#pragma once

#import "Test.h"
#import "WTFStringUtilities.h"
#import <wtf/MainThread.h>

namespace TestWebKitAPI {

class GPULegacy : public testing::Test {
public:
    void SetUp() final
    {
        WTF::initializeMainThread();

        m_librarySourceCode = "using namespace metal;\n\
            struct Vertex\n\
            {\n\
            float4 position [[position]];\n\
            float4 color;\n\
            };\n\
            vertex Vertex vertex_main(device Vertex *vertices [[buffer(0)]],\n\
            uint vid [[vertex_id]])\n\
            {\n\
            return vertices[vid];\n\
            }\n\
            fragment float4 fragment_main(Vertex inVertex [[stage_in]])\n\
            {\n\
            return inVertex.color;\n\
            }"_s;
    }

    const String& librarySourceCode() { return m_librarySourceCode; }

private:
    String m_librarySourceCode;
};

}
