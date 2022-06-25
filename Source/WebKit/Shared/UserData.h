/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#ifndef UserData_h
#define UserData_h

#include "APIObject.h"
#include <wtf/RefPtr.h>

namespace IPC {
class Encoder;
class Decoder;
}

namespace WebKit {

class UserData {
public:
    UserData();
    explicit UserData(RefPtr<API::Object>&&);
    ~UserData();

    struct Transformer {
        virtual ~Transformer() { }
        virtual bool shouldTransformObject(const API::Object&) const = 0;
        virtual RefPtr<API::Object> transformObject(API::Object&) const = 0;
    };
    static RefPtr<API::Object> transform(API::Object*, const Transformer&);

    API::Object* object() const { return m_object.get(); }

    void encode(IPC::Encoder&) const;
    static bool decode(IPC::Decoder&, UserData&) WARN_UNUSED_RETURN;

    static void encode(IPC::Encoder&, const API::Object*);
    static bool decode(IPC::Decoder&, RefPtr<API::Object>&) WARN_UNUSED_RETURN;

private:
    static void encode(IPC::Encoder&, const API::Object&);

    RefPtr<API::Object> m_object;
};

} // namespace WebKit

#endif // UserData_h
