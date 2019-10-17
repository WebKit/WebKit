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
#include "PageGroup.h"

#include "BackForwardCache.h"
#include "DOMWrapperWorld.h"
#include "Document.h"
#include "Frame.h"
#include "Page.h"
#include "StorageNamespace.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <wtf/StdLibExtras.h>

#if ENABLE(VIDEO_TRACK)
#if PLATFORM(MAC) || HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include "CaptionUserPreferencesMediaAF.h"
#else
#include "CaptionUserPreferences.h"
#endif
#endif

namespace WebCore {

static unsigned getUniqueIdentifier()
{
    static unsigned currentIdentifier = 0;
    return ++currentIdentifier;
}

// --------

PageGroup::PageGroup(const String& name)
    : m_name(name)
    , m_identifier(getUniqueIdentifier())
{
}

PageGroup::PageGroup(Page& page)
    : m_identifier(getUniqueIdentifier())
{
    addPage(page);
}

PageGroup::~PageGroup() = default;

typedef HashMap<String, PageGroup*> PageGroupMap;
static PageGroupMap* pageGroups = nullptr;

PageGroup* PageGroup::pageGroup(const String& groupName)
{
    ASSERT(!groupName.isEmpty());
    
    if (!pageGroups)
        pageGroups = new PageGroupMap;

    PageGroupMap::AddResult result = pageGroups->add(groupName, nullptr);

    if (result.isNewEntry) {
        ASSERT(!result.iterator->value);
        result.iterator->value = new PageGroup(groupName);
    }

    ASSERT(result.iterator->value);
    return result.iterator->value;
}

void PageGroup::addPage(Page& page)
{
    ASSERT(!m_pages.contains(&page));
    m_pages.add(&page);
}

void PageGroup::removePage(Page& page)
{
    ASSERT(m_pages.contains(&page));
    m_pages.remove(&page);
}

#if ENABLE(VIDEO_TRACK)
void PageGroup::captionPreferencesChanged()
{
    for (auto& page : m_pages)
        page->captionPreferencesChanged();
    BackForwardCache::singleton().markPagesForCaptionPreferencesChanged();
}

CaptionUserPreferences& PageGroup::captionPreferences()
{
    if (!m_captionPreferences) {
#if PLATFORM(MAC) || HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
        m_captionPreferences = makeUnique<CaptionUserPreferencesMediaAF>(*this);
#else
        m_captionPreferences = makeUnique<CaptionUserPreferences>(*this);
#endif
    }

    return *m_captionPreferences.get();
}
#endif

} // namespace WebCore
