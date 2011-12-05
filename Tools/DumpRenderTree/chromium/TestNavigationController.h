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

#ifndef TestNavigationController_h
#define TestNavigationController_h

#include "WebDataSource.h"
#include "WebHistoryItem.h"
#include "platform/WebString.h"
#include "platform/WebURL.h"
#include "webkit/support/webkit_support.h"
#include <string>
#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>
#include <wtf/Vector.h>

// Associated with browser-initated navigations to hold tracking data.
class TestShellExtraData : public WebKit::WebDataSource::ExtraData {
public:
    TestShellExtraData(int32_t pendingPageID)
        : pendingPageID(pendingPageID)
        , requestCommitted(false) { }

    // Contains the page_id for this navigation or -1 if there is none yet.
    int32_t pendingPageID;

    // True if we have already processed the "DidCommitLoad" event for this
    // request. Used by session history.
    bool requestCommitted;
};

// Stores one back/forward navigation state for the test shell.
class TestNavigationEntry: public RefCounted<TestNavigationEntry> {
public:
    static PassRefPtr<TestNavigationEntry> create();
    static PassRefPtr<TestNavigationEntry> create(
        int pageID,
        const WebKit::WebURL&,
        const WebKit::WebString& title,
        const WebKit::WebString& targetFrame);

    // Virtual to allow test_shell to extend the class.
    virtual ~TestNavigationEntry();

    // Set / Get the URI
    void setURL(const WebKit::WebURL& url) { m_url = url; }
    const WebKit::WebURL& URL() const { return m_url; }

    // Set / Get the title
    void setTitle(const WebKit::WebString& title) { m_title = title; }
    const WebKit::WebString& title() const { return m_title; }

    // Set / Get a state.
    void setContentState(const WebKit::WebHistoryItem&);
    const WebKit::WebHistoryItem& contentState() const { return m_state; }

    // Get the page id corresponding to the tab's state.
    void setPageID(int pageID) { m_pageID = pageID; }
    int32_t pageID() const { return m_pageID; }

    const WebKit::WebString& targetFrame() const { return m_targetFrame; }

private:
    TestNavigationEntry();
    TestNavigationEntry(int pageID,
                        const WebKit::WebURL&,
                        const WebKit::WebString& title,
                        const WebKit::WebString& targetFrame);

    // Describes the current page that the tab represents. This is not relevant
    // for all tab contents types.
    int32_t m_pageID;

    WebKit::WebURL m_url;
    WebKit::WebString m_title;
    WebKit::WebHistoryItem m_state;
    WebKit::WebString m_targetFrame;
};

class NavigationHost {
public:
    virtual bool navigate(const TestNavigationEntry&, bool reload) = 0;
};

// Test shell's NavigationController. The goal is to be as close to the Chrome
// version as possible.
class TestNavigationController {
    WTF_MAKE_NONCOPYABLE(TestNavigationController);
public:
    TestNavigationController(NavigationHost*);
    ~TestNavigationController();

    void reset();

    // Causes the controller to reload the current (or pending) entry.
    void reload();

    // Causes the controller to go to the specified offset from current. Does
    // nothing if out of bounds.
    void goToOffset(int);

    // Causes the controller to go to the specified index.
    void goToIndex(int);

    // Causes the controller to load the specified entry.
    // NOTE: Do not pass an entry that the controller already owns!
    void loadEntry(TestNavigationEntry*);

    // Returns the last committed entry, which may be null if there are no
    // committed entries.
    TestNavigationEntry* lastCommittedEntry() const;

    // Returns the number of entries in the NavigationControllerBase, excluding
    // the pending entry if there is one.
    int entryCount() const { return static_cast<int>(m_entries.size()); }

    // Returns the active entry, which is the pending entry if a navigation is in
    // progress or the last committed entry otherwise. NOTE: This can be 0!!
    //
    // If you are trying to get the current state of the NavigationControllerBase,
    // this is the method you will typically want to call.
    TestNavigationEntry* activeEntry() const;

    // Returns the index from which we would go back/forward or reload. This is
    // the m_lastCommittedEntryIndex if m_pendingEntryIndex is -1. Otherwise,
    // it is the m_pendingEntryIndex.
    int currentEntryIndex() const;

    // Returns the entry at the specified index. Returns 0 if out of bounds.
    TestNavigationEntry* entryAtIndex(int) const;

    // Return the entry with the corresponding type and page ID, or 0 if
    // not found.
    TestNavigationEntry* entryWithPageID(int32_t) const;

    // Returns the index of the last committed entry.
    int lastCommittedEntryIndex() const { return m_lastCommittedEntryIndex; }

    // Used to inform us of a navigation being committed for a tab. Any entry
    // located forward to the current entry will be deleted. The new entry
    // becomes the current entry.
    void didNavigateToEntry(TestNavigationEntry*);

    // Used to inform us to discard its pending entry.
    void discardPendingEntry();

private:
    // Inserts an entry after the current position, removing all entries after it.
    // The new entry will become the active one.
    void insertEntry(TestNavigationEntry*);

    int maxPageID() const { return m_maxPageID; }
    void navigateToPendingEntry(bool reload);

    // Return the index of the entry with the corresponding type and page ID,
    // or -1 if not found.
    int entryIndexWithPageID(int32_t) const;

    // Updates the max page ID with that of the given entry, if is larger.
    void updateMaxPageID();

    // List of NavigationEntry for this tab
    typedef Vector<RefPtr<TestNavigationEntry> > NavigationEntryList;
    typedef NavigationEntryList::iterator NavigationEntryListIterator;
    NavigationEntryList m_entries;

    // An entry we haven't gotten a response for yet. This will be discarded
    // when we navigate again. It's used only so we know what the currently
    // displayed tab is.
    RefPtr<TestNavigationEntry> m_pendingEntry;

    // currently visible entry
    int m_lastCommittedEntryIndex;

    // index of pending entry if it is in entries_, or -1 if pending_entry_ is a
    // new entry (created by LoadURL).
    int m_pendingEntryIndex;

    NavigationHost* m_host;
    int m_maxPageID;
};

#endif // TestNavigationController_h

