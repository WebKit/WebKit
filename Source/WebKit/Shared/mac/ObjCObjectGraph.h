/*
 * Copyright (C) 2012-2020 Apple Inc. All rights reserved.
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

#include "APIObject.h"
#include <wtf/RetainPtr.h>

namespace IPC {
class Decoder;
class Encoder;
}

typedef struct objc_object* id;

namespace WebKit {

class ObjCObjectGraph : public API::ObjectImpl<API::Object::Type::ObjCObjectGraph> {
public:
    static Ref<ObjCObjectGraph> create(id rootObject)
    {
        return adoptRef(*new ObjCObjectGraph(rootObject));
    }

    id rootObject() const { return m_rootObject.get(); }

    struct Transformer {
        virtual ~Transformer() { }
        virtual bool shouldTransformObject(id) const = 0;
        virtual RetainPtr<id> transformObject(id) const = 0;
    };
    static RetainPtr<id> transform(id, const Transformer&);

    void encode(IPC::Encoder&) const;
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, RefPtr<API::Object>&);

private:
    static void encode(IPC::Encoder&, id);
    static WARN_UNUSED_RETURN bool decode(IPC::Decoder&, RetainPtr<id>&);

    explicit ObjCObjectGraph(id rootObject)
        : m_rootObject(rootObject)
    {
    }

    RetainPtr<id> m_rootObject;
};

} // namespace WebKit
