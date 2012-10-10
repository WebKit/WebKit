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

#include "config.h"
#include "UserContentContainer.h"

#include "ArgumentCoders.h"
#include "ArgumentEncoder.h"
#include "Arguments.h"

namespace WebKit {

UserContentContainer::Item::Item()
{
}

UserContentContainer::Item::Item(PassRefPtr<WebString> source, PassRefPtr<WebURL> url, PassRefPtr<ImmutableArray> whitelist, PassRefPtr<ImmutableArray> blacklist, InjectedFrames injectedFrames, Type type)
    : m_source(source)
    , m_url(url)
    , m_whitelist(whitelist)
    , m_blacklist(blacklist)
    , m_injectedFrames(injectedFrames)
    , m_type(type)
{
    ASSERT(m_source);
}

static void encodeStringArray(CoreIPC::ArgumentEncoder* encoder, ImmutableArray* array)
{
    ASSERT(encoder);
    uint32_t size = array ? array->size() : 0;
    encoder->encode(size);
    for (uint32_t i = 0; i < size; ++i) {
        WebString* webString = array->at<WebString>(i);
        ASSERT(webString);
        encoder->encode(webString->string());
    }
}

void UserContentContainer::Item::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_source->string());
    encoder->encode(m_url ? m_url->string() : String());
    encodeStringArray(encoder, m_whitelist.get());
    encodeStringArray(encoder, m_blacklist.get());
    encoder->encode(static_cast<uint32_t>(m_injectedFrames));
    encoder->encode(static_cast<uint32_t>(m_type));
}

static bool decodeStringArray(CoreIPC::ArgumentDecoder* decoder, Vector<RefPtr<APIObject> >& vector)
{
    ASSERT(decoder);
    ASSERT(vector.isEmpty());

    uint32_t size;
    if (!decoder->decode(size))
        return false;

    if (!size)
        return true;

    vector.reserveInitialCapacity(size);
    for (uint32_t i = 0; i < size; ++i) {
        String string;
        if (!decoder->decode(string))
            return false;
        vector.uncheckedAppend(WebString::create(string));
    }

    return true;
}

bool UserContentContainer::Item::decode(CoreIPC::ArgumentDecoder* decoder, UserContentContainer::Item& item)
{
    ASSERT(!item.m_source);
    ASSERT(!item.m_url);
    ASSERT(!item.m_whitelist);
    ASSERT(!item.m_blacklist);

    String source;
    if (!decoder->decode(source))
        return false;
    item.m_source = WebString::create(source);

    String url;
    if (!decoder->decode(url))
        return false;
    item.m_url = WebURL::create(url);

    Vector<RefPtr<APIObject> > whitelist;
    if (!decodeStringArray(decoder, whitelist))
        return false;
    item.m_whitelist = ImmutableArray::adopt(whitelist);

    Vector<RefPtr<APIObject> > blacklist;
    if (!decodeStringArray(decoder, blacklist))
        return false;
    item.m_blacklist = ImmutableArray::adopt(blacklist);

    uint32_t injectedFrames;
    if (!decoder->decode(injectedFrames))
        return false;
    item.m_injectedFrames = static_cast<InjectedFrames>(injectedFrames);

    uint32_t type;
    if (!decoder->decode(type))
        return false;
    item.m_type = static_cast<Type>(type);

    return true;
}

static PassOwnPtr<Vector<String> > toStringVector(ImmutableArray* array)
{
    if (!array)
        return nullptr;

    size_t size = array->size();
    if (!size)
        return nullptr;
    
    OwnPtr<Vector<String> > stringVector = adoptPtr(new Vector<String>());
    stringVector->reserveInitialCapacity(size);
    for (size_t i = 0; i < size; ++i) {
        WebString* webString = array->at<WebString>(i);
        ASSERT(webString);
        stringVector->uncheckedAppend(webString->string());
    }
    
    return stringVector.release();
}

PassOwnPtr<Vector<String> > UserContentContainer::Item::whitelist() const
{
    return toStringVector(m_whitelist.get());
}

PassOwnPtr<Vector<String> > UserContentContainer::Item::blacklist() const
{
    return toStringVector(m_blacklist.get());
}

void UserContentContainer::encode(CoreIPC::ArgumentEncoder* encoder) const
{
    encoder->encode(m_userContent);
}

bool UserContentContainer::decode(CoreIPC::ArgumentDecoder* decoder, UserContentContainer& userContentContainer)
{
    ASSERT(userContentContainer.m_userContent.isEmpty());
    if (!decoder->decode(userContentContainer.m_userContent))
        return false;
    return true;
}

} // namespace WebKit
