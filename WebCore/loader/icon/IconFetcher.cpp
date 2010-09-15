/*
 * Copyright (C) 2008 Apple Inc. All Rights Reserved.
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

#include "config.h"
#include "IconFetcher.h"

#include "Frame.h"
#include "FrameLoaderClient.h"
#include "HTMLHeadElement.h"
#include "HTMLLinkElement.h"
#include "HTMLNames.h"
#include "ResourceHandle.h"
#include "ResourceRequest.h"
#include "SharedBuffer.h"

namespace WebCore {

using namespace HTMLNames;

struct IconLinkEntry {
public:
    enum IconType {
        Unknown,
        ICNS,
        ICO,
    };
    
    IconLinkEntry(IconType type, const KURL& url) 
        : m_type(type)
        , m_url(url)
    {
    }
    
    IconType type() const { return m_type; }
    const KURL& url() const { return m_url; }

    SharedBuffer* buffer() 
    {
        if (!m_buffer)
            m_buffer = SharedBuffer::create();
        
        return m_buffer.get();
    }
    
private:
    RefPtr<SharedBuffer> m_buffer;
    IconType m_type;
    KURL m_url;
};
    
#if PLATFORM(MAC)
static const IconLinkEntry::IconType NativeIconType = IconLinkEntry::ICNS;
#elif PLATFORM(WIN)
static const IconLinkEntry::IconType NativeIconType = IconLinkEntry::ICO;
#else
static const IconLinkEntry::IconType NativeIconType = IconLinkEntry::Unknown;
#endif

static void parseIconLink(HTMLLinkElement* link, Vector<IconLinkEntry>& entries)
{
    // FIXME: Parse the size attribute too.
    
    IconLinkEntry::IconType type = IconLinkEntry::Unknown;
    const KURL& url = link->href();

    // Try to determine the file type.
    String path = url.path();
    
    size_t pos = path.reverseFind('.');
    if (pos != notFound) {
        String extension = path.substring(pos + 1);
        if (equalIgnoringCase(extension, "icns"))
            type = IconLinkEntry::ICNS;
        else if (equalIgnoringCase(extension, "ico"))
            type = IconLinkEntry::ICO;
    }
    
    entries.append(IconLinkEntry(type, url));
}
    
PassRefPtr<IconFetcher> IconFetcher::create(Frame* frame, IconFetcherClient* client)
{
    Document* document = frame->document();
    
    HTMLHeadElement* head = document->head();
    if (!head)
        return 0;
    
    Vector<IconLinkEntry> entries;
    
    for (Node* n = head; n; n = n->traverseNextNode()) {
        if (!n->hasTagName(linkTag))    
            continue;
            
        HTMLLinkElement* link = static_cast<HTMLLinkElement*>(n);
        if (!link->isIcon())
            continue;

        parseIconLink(link, entries);
    }
    
    if (entries.isEmpty())
        return 0;
    
    // Check if any of the entries have the same type as the native icon type.

    // FIXME: This should be way more sophisticated, and handle conversion
    // of multisize formats for example.
    for (unsigned i = 0; i < entries.size(); i++) {
        const IconLinkEntry& entry = entries[i];
        if (entry.type() == NativeIconType) {
            RefPtr<IconFetcher> iconFetcher = adoptRef(new IconFetcher(frame, client));
            
            iconFetcher->m_entries.append(entry);
            iconFetcher->loadEntry();
            
            return iconFetcher.release();
        }
    }

    return 0;
}    

IconFetcher::IconFetcher(Frame* frame, IconFetcherClient* client)
    : m_frame(frame)
    , m_client(client)
    , m_currentEntry(0)
{
}
    
IconFetcher::~IconFetcher()
{
    cancel();
}

void IconFetcher::cancel()
{
    if (m_handle)
        m_handle->cancel();
}

PassRefPtr<SharedBuffer> IconFetcher::createIcon()
{
    ASSERT(!m_entries.isEmpty());
    
    // For now, just return the data of the first entry.
    return m_entries.first().buffer();
}

void IconFetcher::loadEntry()
{
    ASSERT(m_currentEntry < m_entries.size());
    ASSERT(!m_handle);
    
    m_handle = ResourceHandle::create(m_frame->loader()->networkingContext(), m_entries[m_currentEntry].url(), this, false, false);
}
    
void IconFetcher::loadFailed()
{
    m_handle = 0;
    
    m_client->finishedFetchingIcon(0);
}    
    
void IconFetcher::didReceiveResponse(ResourceHandle* handle, const ResourceResponse& response)
{
    ASSERT_UNUSED(handle, m_handle == handle);
    
    int statusCode = response.httpStatusCode() / 100;
    if (statusCode == 4 || statusCode == 5) {
        loadFailed();
        return;
    }    
}
    
void IconFetcher::didReceiveData(ResourceHandle* handle, const char* data, int length, int)
{
    ASSERT_UNUSED(handle, m_handle == handle);
    
    m_entries[m_currentEntry].buffer()->append(data, length);
}

void IconFetcher::didFinishLoading(ResourceHandle* handle, double)
{
    ASSERT_UNUSED(handle, m_handle == handle);
    
    if (m_currentEntry == m_entries.size() - 1) {
        // We finished loading, create the icon
        RefPtr<SharedBuffer> iconData = createIcon();
        
        m_client->finishedFetchingIcon(iconData.release());
        return;
    }
    
    // Load the next entry
    m_currentEntry++;

    loadEntry();
}
    
void IconFetcher::didFail(ResourceHandle* handle, const ResourceError&)
{
    ASSERT_UNUSED(handle, m_handle == handle);
    
    loadFailed();
}

} // namespace WebCore
