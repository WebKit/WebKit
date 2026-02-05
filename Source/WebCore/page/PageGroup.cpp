/*
 * Copyright (C) 2008 Apple Inc. All rights reserved.
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
#include "LocalFrame.h"
#include "Page.h"
#include "StorageNamespace.h"
#include <JavaScriptCore/HeapInlines.h>
#include <JavaScriptCore/StructureInlines.h>
#include <wtf/StdLibExtras.h>
#include <wtf/TZoneMallocInlines.h>

#if ENABLE(VIDEO)
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
#include "CaptionUserPreferencesMediaAF.h"
#else
#include "CaptionUserPreferences.h"
#endif
#endif

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(PageGroup);

static unsigned getUniqueIdentifier()
{
    static unsigned currentIdentifier = 0;
    return ++currentIdentifier;
}

// --------

UniqueRef<PageGroup> PageGroup::create(const String& name)
{
    return UniqueRef<PageGroup>(*new PageGroup(name));
}

UniqueRef<PageGroup> PageGroup::create(Page& page)
{
    return UniqueRef<PageGroup>(*new PageGroup(page));
}

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

using PageGroupMap = HashMap<String, UniqueRef<PageGroup>>;

static PageGroupMap& pageGroups()
{
    static NeverDestroyed<PageGroupMap> pageGroupsMap;
    return pageGroupsMap;
}

PageGroup* PageGroup::pageGroup(const String& groupName)
{
    ASSERT(!groupName.isEmpty());

    return pageGroups().ensure(groupName, [&] {
        return PageGroup::create(groupName);
    }).iterator->value.ptr();
}

void PageGroup::addPage(Page& page)
{
    ASSERT(!m_pages.contains(page));
    m_pages.add(page);
}

void PageGroup::removePage(Page& page)
{
    ASSERT(m_pages.contains(page));
    m_pages.remove(page);
}

#if ENABLE(VIDEO)
void PageGroup::captionPreferencesChanged()
{
    for (Ref page : m_pages)
        page->captionPreferencesChanged();
    BackForwardCache::singleton().markPagesForCaptionPreferencesChanged();
}

CaptionUserPreferences& PageGroup::ensureCaptionPreferences()
{
    if (!m_captionPreferences) {
#if HAVE(MEDIA_ACCESSIBILITY_FRAMEWORK)
        lazyInitialize(m_captionPreferences, CaptionUserPreferencesMediaAF::create(*this));
#else
        lazyInitialize(m_captionPreferences, CaptionUserPreferences::create(*this));
#endif
    }

    return *m_captionPreferences;
}

Ref<CaptionUserPreferences> PageGroup::ensureProtectedCaptionPreferences()
{
    return ensureCaptionPreferences();
}
#endif

} // namespace WebCore
