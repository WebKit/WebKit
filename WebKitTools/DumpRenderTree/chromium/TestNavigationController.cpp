/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "TestNavigationController.h"

#include "TestShell.h"
#include <wtf/Assertions.h>

using namespace WebKit;
using namespace std;

// ----------------------------------------------------------------------------
// TestNavigationEntry

PassRefPtr<TestNavigationEntry> TestNavigationEntry::create()
{
    return adoptRef(new TestNavigationEntry);
}

PassRefPtr<TestNavigationEntry> TestNavigationEntry::create(
    int pageID, const WebURL& url, const WebString& title, const WebString& targetFrame)
{
    return adoptRef(new TestNavigationEntry(pageID, url, title, targetFrame));
}

TestNavigationEntry::TestNavigationEntry()
    : m_pageID(-1) {}

TestNavigationEntry::TestNavigationEntry(
    int pageID, const WebURL& url, const WebString& title, const WebString& targetFrame)
    : m_pageID(pageID)
    , m_url(url)
    , m_title(title)
    , m_targetFrame(targetFrame) {}

TestNavigationEntry::~TestNavigationEntry() {}

void TestNavigationEntry::setContentState(const WebHistoryItem& state)
{
    m_state = state;
}

// ----------------------------------------------------------------------------
// TestNavigationController

TestNavigationController::TestNavigationController(NavigationHost* host)
    : m_pendingEntry(0)
    , m_lastCommittedEntryIndex(-1)
    , m_pendingEntryIndex(-1)
    , m_host(host)
    , m_maxPageID(-1) {}

TestNavigationController::~TestNavigationController()
{
    discardPendingEntry();
}

void TestNavigationController::reset()
{
    m_entries.clear();
    discardPendingEntry();

    m_lastCommittedEntryIndex = -1;
}

void TestNavigationController::reload()
{
    // Base the navigation on where we are now...
    int currentIndex = currentEntryIndex();

    // If we are no where, then we can't reload.  TODO(darin): We should add a
    // CanReload method.
    if (currentIndex == -1)
        return;

    discardPendingEntry();

    m_pendingEntryIndex = currentIndex;
    navigateToPendingEntry(true);
}

void TestNavigationController::goToOffset(int offset)
{
    int index = m_lastCommittedEntryIndex + offset;
    if (index < 0 || index >= entryCount())
        return;

    goToIndex(index);
}

void TestNavigationController::goToIndex(int index)
{
    ASSERT(index >= 0);
    ASSERT(index < static_cast<int>(m_entries.size()));

    discardPendingEntry();

    m_pendingEntryIndex = index;
    navigateToPendingEntry(false);
}

void TestNavigationController::loadEntry(TestNavigationEntry* entry)
{
    // When navigating to a new page, we don't know for sure if we will actually
    // end up leaving the current page.  The new page load could for example
    // result in a download or a 'no content' response (e.g., a mailto: URL).
    discardPendingEntry();
    m_pendingEntry = entry;
    navigateToPendingEntry(false);
}


TestNavigationEntry* TestNavigationController::lastCommittedEntry() const
{
    if (m_lastCommittedEntryIndex == -1)
        return 0;
    return m_entries[m_lastCommittedEntryIndex].get();
}

TestNavigationEntry* TestNavigationController::activeEntry() const
{
    TestNavigationEntry* entry = m_pendingEntry.get();
    if (!entry)
        entry = lastCommittedEntry();
    return entry;
}

int TestNavigationController::currentEntryIndex() const
{
    if (m_pendingEntryIndex != -1)
        return m_pendingEntryIndex;
    return m_lastCommittedEntryIndex;
}


TestNavigationEntry* TestNavigationController::entryAtIndex(int index) const
{
    if (index < 0 || index >= entryCount())
        return 0;
    return m_entries[index].get();
}

TestNavigationEntry* TestNavigationController::entryWithPageID(int32_t pageID) const
{
    int index = entryIndexWithPageID(pageID);
    return (index != -1) ? m_entries[index].get() : 0;
}

void TestNavigationController::didNavigateToEntry(TestNavigationEntry* entry)
{
    // If the entry is that of a page with PageID larger than any this Tab has
    // seen before, then consider it a new navigation.
    if (entry->pageID() > maxPageID()) {
        insertEntry(entry);
        return;
    }

    // Otherwise, we just need to update an existing entry with matching PageID.
    // If the existing entry corresponds to the entry which is pending, then we
    // must update the current entry index accordingly.  When navigating to the
    // same URL, a new PageID is not created.

    int existingEntryIndex = entryIndexWithPageID(entry->pageID());
    TestNavigationEntry* existingEntry = (existingEntryIndex != -1) ?
        m_entries[existingEntryIndex].get() : 0;
    if (!existingEntry) {
        // No existing entry, then simply ignore this navigation!
    } else if (existingEntry == m_pendingEntry.get()) {
        // The given entry might provide a new URL... e.g., navigating back to a
        // page in session history could have resulted in a new client redirect.
        existingEntry->setURL(entry->URL());
        existingEntry->setContentState(entry->contentState());
        m_lastCommittedEntryIndex = m_pendingEntryIndex;
        m_pendingEntryIndex = -1;
        m_pendingEntry.clear();
    } else if (m_pendingEntry && m_pendingEntry->pageID() == -1
               && GURL(m_pendingEntry->URL()) == GURL(existingEntry->URL().spec())) {
        // Not a new navigation
        discardPendingEntry();
    } else {
        // The given entry might provide a new URL... e.g., navigating to a page
        // might result in a client redirect, which should override the URL of the
        // existing entry.
        existingEntry->setURL(entry->URL());
        existingEntry->setContentState(entry->contentState());

        // The navigation could have been issued by the renderer, so be sure that
        // we update our current index.
        m_lastCommittedEntryIndex = existingEntryIndex;
    }

    updateMaxPageID();
}

void TestNavigationController::discardPendingEntry()
{
    m_pendingEntry.clear();
    m_pendingEntryIndex = -1;
}

void TestNavigationController::insertEntry(TestNavigationEntry* entry)
{
    discardPendingEntry();

    // Prune any entry which are in front of the current entry
    int currentSize = static_cast<int>(m_entries.size());
    if (currentSize > 0) {
        while (m_lastCommittedEntryIndex < (currentSize - 1)) {
            m_entries.removeLast();
            currentSize--;
        }
    }

    m_entries.append(RefPtr<TestNavigationEntry>(entry));
    m_lastCommittedEntryIndex = static_cast<int>(m_entries.size()) - 1;
    updateMaxPageID();
}

int TestNavigationController::entryIndexWithPageID(int32 pageID) const
{
    for (int i = static_cast<int>(m_entries.size()) - 1; i >= 0; --i) {
        if (m_entries[i]->pageID() == pageID)
            return i;
    }
    return -1;
}

void TestNavigationController::navigateToPendingEntry(bool reload)
{
    // For session history navigations only the pending_entry_index_ is set.
    if (!m_pendingEntry) {
        ASSERT(m_pendingEntryIndex != -1);
        m_pendingEntry = m_entries[m_pendingEntryIndex];
    }

    if (m_host->navigate(*m_pendingEntry.get(), reload)) {
        // Note: this is redundant if navigation completed synchronously because
        // DidNavigateToEntry call this as well.
        updateMaxPageID();
    } else
        discardPendingEntry();
}

void TestNavigationController::updateMaxPageID()
{
    TestNavigationEntry* entry = activeEntry();
    if (entry)
        m_maxPageID = max(m_maxPageID, entry->pageID());
}
