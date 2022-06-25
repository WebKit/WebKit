/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#include <stdint.h>
#include <wtf/Forward.h>

namespace IPC {
class Decoder;
class Encoder;
}

namespace WebKit {

enum class LayerHostingMode : uint8_t {
    InProcess,
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
    OutOfProcess
#endif
};

class LayerTreeContext {
public:
    LayerTreeContext();
    ~LayerTreeContext();

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, LayerTreeContext&);

    bool isEmpty() const;

    uint64_t contextID;
};

bool operator==(const LayerTreeContext&, const LayerTreeContext&);

inline bool operator!=(const LayerTreeContext& a, const LayerTreeContext& b)
{
    return !(a == b);
}

}

namespace WTF {
template<> struct EnumTraits<WebKit::LayerHostingMode> {
    using values = EnumValues<
        WebKit::LayerHostingMode,
#if HAVE(OUT_OF_PROCESS_LAYER_HOSTING)
        WebKit::LayerHostingMode::OutOfProcess,
#endif
        WebKit::LayerHostingMode::InProcess
    >;
};
}
