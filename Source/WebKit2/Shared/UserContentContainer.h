/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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

#ifndef UserContentContainer_h
#define UserContentContainer_h

#include "ImmutableArray.h"
#include "WebString.h"
#include "WebURL.h"
#include <WebCore/KURL.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

namespace CoreIPC {
class ArgumentDecoder;
class ArgumentEncoder;
}

namespace WebKit {

class UserContentContainer {
public:
    class Item {
    public:
        enum Type {
            TypeStylesheet,
            TypeScript
        };
        
        enum InjectedFrames {
            InjectInAllFrames,
            InjectInTopFrameOnly
        };

        Item();
        Item(PassRefPtr<WebString> source, PassRefPtr<WebURL>, PassRefPtr<ImmutableArray> whitelist, PassRefPtr<ImmutableArray> blacklist, InjectedFrames, Type);

        void encode(CoreIPC::ArgumentEncoder*) const;
        static bool decode(CoreIPC::ArgumentDecoder*, Item&);
        
        const String& source() const { return m_source->string(); }
        WebCore::KURL url() const { return !m_url || m_url->string().isEmpty() ? WebCore::blankURL() : WebCore::KURL(WebCore::KURL(), m_url->string()); }
        const Vector<String>& whitelist() const { return m_whitelist; }
        const Vector<String>& blacklist() const { return m_blacklist; }
        InjectedFrames injectedFrames() const { return m_injectedFrames; }
        Type type() const { return m_type; }

    private:
        RefPtr<WebString> m_source;
        RefPtr<WebURL> m_url;
        Vector<String> m_whitelist;
        Vector<String> m_blacklist;
        InjectedFrames m_injectedFrames;
        Type m_type;
    };

    void encode(CoreIPC::ArgumentEncoder*) const;
    static bool decode(CoreIPC::ArgumentDecoder*, UserContentContainer&);
    
    void addItem(const Item& item) { m_userContent.append(item); }
    void removeAllItems() { m_userContent.clear(); }
    
    size_t size() const { return m_userContent.size(); }
    const Item& at(size_t i) const { return m_userContent.at(i); }

private:
    Vector<UserContentContainer::Item> m_userContent;
};

} // namespace WebKit

#endif // UserContentContainer
