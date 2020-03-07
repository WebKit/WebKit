/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#pragma once

#include "DisplayList.h"
#include <wtf/Noncopyable.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

enum class AlphaPremultiplication : uint8_t;
class FloatRect;
class GraphicsContext;
class ImageData;

namespace DisplayList {

class Replayer {
    WTF_MAKE_NONCOPYABLE(Replayer);
public:
    class Delegate;
    WEBCORE_EXPORT Replayer(GraphicsContext&, const DisplayList&, Delegate* = nullptr);
    WEBCORE_EXPORT ~Replayer();

    WEBCORE_EXPORT std::unique_ptr<DisplayList> replay(const FloatRect& initialClip = { }, bool trackReplayList = false);

    class Delegate {
    public:
        virtual ~Delegate() { }
        virtual bool apply(Item&, GraphicsContext&) { return false; }
    };
    
private:
    const DisplayList& m_displayList;
    GraphicsContext& m_context;
    Delegate* m_delegate;
};

}
}

