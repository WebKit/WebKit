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

#ifndef APIUserContentExtension_h
#define APIUserContentExtension_h

#include "APIObject.h"
#include <wtf/text/WTFString.h>

namespace WebKit {
class WebCompiledContentExtension;
}

namespace API {

class UserContentExtension final : public ObjectImpl<Object::Type::UserContentExtension> {
public:
#if ENABLE(CONTENT_EXTENSIONS)
    static Ref<UserContentExtension> create(const WTF::String& name, Ref<WebKit::WebCompiledContentExtension>&& contentExtension)
    {
        return adoptRef(*new UserContentExtension(name, WTFMove(contentExtension)));
    }

    UserContentExtension(const WTF::String& name, Ref<WebKit::WebCompiledContentExtension>&&);
    virtual ~UserContentExtension();

    const WTF::String& name() const { return m_name; }
    const WebKit::WebCompiledContentExtension& compiledExtension() const { return m_compiledExtension.get(); }

private:
    WTF::String m_name;
    Ref<WebKit::WebCompiledContentExtension> m_compiledExtension;
#endif // ENABLE(CONTENT_EXTENSIONS)
};

} // namespace API

#endif // APIUserContentExtension_h
