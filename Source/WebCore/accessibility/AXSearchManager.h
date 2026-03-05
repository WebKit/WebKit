/*
 * Copyright (C) 2024 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/AXCoreObject.h>
#include <WebCore/AccessibilityRemoteToken.h>
#include <WebCore/FrameIdentifier.h>
#include <wtf/HashMap.h>
#include <wtf/MonotonicTime.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

DECLARE_ALLOCATOR_WITH_HEAP_IDENTIFIER(AXSearchManager);

enum class AccessibilitySearchKey : uint8_t {
    AnyType = 1,
    Article,
    BlockquoteSameLevel,
    Blockquote,
    BoldFont,
    Button,
    Checkbox,
    Control,
    DifferentType,
    FontChange,
    FontColorChange,
    Frame,
    Graphic,
    HeadingLevel1,
    HeadingLevel2,
    HeadingLevel3,
    HeadingLevel4,
    HeadingLevel5,
    HeadingLevel6,
    HeadingSameLevel,
    Heading,
    Highlighted,
    ItalicFont,
    KeyboardFocusable,
    Landmark,
    Link,
    List,
    LiveRegion,
    MisspelledWord,
    Outline,
    PlainText,
    RadioGroup,
    SameType,
    StaticText,
    StyleChange,
    TableSameLevel,
    Table,
    TextField,
    Underline,
    UnvisitedLink,
    VisitedLink,
};

struct AccessibilitySearchCriteria {
    // FIXME: change the object pointers to object IDs.
    WeakPtr<AXCoreObject> anchorObject { nullptr };
    WeakPtr<AXCoreObject> startObject { nullptr };
    CharacterRange startRange;
    AccessibilitySearchDirection searchDirection { AccessibilitySearchDirection::Next };
    Vector<AccessibilitySearchKey> searchKeys;
    String searchText;
    unsigned resultsLimit { 0 };
    bool visibleOnly { false };
    bool immediateDescendantsOnly { false };
};

// IPC-serializable version of AccessibilitySearchCriteria for cross-process search queries.
// Excludes object pointers since the remote frame will use its own root as anchor.
struct AccessibilitySearchCriteriaIPC {
    AccessibilitySearchDirection searchDirection { AccessibilitySearchDirection::Next };
    Vector<AccessibilitySearchKey> searchKeys;
    String searchText;
    unsigned resultsLimit { 0 };
    bool visibleOnly { false };
    bool immediateDescendantsOnly { false };
    // Absolute deadline for the top-level search. Used to implement cascading timeouts
    // so deeply nested frames don't each use their own full timeout budget.
    std::optional<MonotonicTime> deadline;

    AccessibilitySearchCriteriaIPC() = default;

    // Create from a regular AccessibilitySearchCriteria for IPC transmission
    explicit AccessibilitySearchCriteriaIPC(const AccessibilitySearchCriteria& criteria)
        : searchDirection(criteria.searchDirection)
        , searchKeys(criteria.searchKeys)
        , searchText(criteria.searchText)
        , resultsLimit(criteria.resultsLimit)
        , visibleOnly(criteria.visibleOnly)
        , immediateDescendantsOnly(criteria.immediateDescendantsOnly)
    { }

    // Constructor for IPC deserialization
    AccessibilitySearchCriteriaIPC(AccessibilitySearchDirection direction, Vector<AccessibilitySearchKey> keys, String text, unsigned limit, bool visible, bool immediateDescendants, std::optional<MonotonicTime> deadline)
        : searchDirection(direction)
        , searchKeys(WTF::move(keys))
        , searchText(WTF::move(text))
        , resultsLimit(limit)
        , visibleOnly(visible)
        , immediateDescendantsOnly(immediateDescendants)
        , deadline(deadline)
    { }

    // Convert back to a full AccessibilitySearchCriteria with a given anchor object
    AccessibilitySearchCriteria toSearchCriteria(AXCoreObject* anchorObject) const
    {
        return {
            anchorObject,
            nullptr, // startObject - start from beginning of anchor
            { }, // startRange
            searchDirection,
            searchKeys,
            searchText,
            resultsLimit,
            visibleOnly,
            immediateDescendantsOnly
        };
    }
};

// Represents a single entry in the search result stream.
// Can be either a local result or a placeholder for a remote frame.
class SearchResultEntry {
public:
    enum class Type : uint8_t { LocalResult, RemoteFrame };

    static SearchResultEntry localResult(Ref<AXCoreObject> object, size_t index)
    {
        return { Type::LocalResult, object.ptr(), std::nullopt, index };
    }

    static SearchResultEntry remoteFrame(FrameIdentifier fid, size_t index)
    {
        return { Type::RemoteFrame, nullptr, fid, index };
    }

    bool isLocalResult() const { return m_type == Type::LocalResult; }
    bool isRemoteFrame() const { return m_type == Type::RemoteFrame; }

    RefPtr<AXCoreObject> objectIfLocalResult() const { return isLocalResult() ? m_object : nullptr; }
    const std::optional<FrameIdentifier>& frameID() const { return m_frameID; }
    size_t streamIndex() const { return m_streamIndex; }

private:
    SearchResultEntry(Type type, RefPtr<AXCoreObject> object, std::optional<FrameIdentifier> frameID, size_t streamIndex)
        : m_type(type)
        , m_object(WTF::move(object))
        , m_frameID(WTF::move(frameID))
        , m_streamIndex(streamIndex)
    {
        // A local result must have an object; a remote frame must have a frameID.
        AX_ASSERT((m_type == Type::LocalResult) == (m_object != nullptr));
        AX_ASSERT((m_type == Type::RemoteFrame) == m_frameID.has_value());
    }

    Type m_type { Type::LocalResult };
    RefPtr<AXCoreObject> m_object;
    std::optional<FrameIdentifier> m_frameID;
    size_t m_streamIndex { 0 };
};

// Result of a search with entries in tree traversal order.
// This allows proper interleaving of local and remote results.
class AccessibilitySearchResultStream {
public:
    // Appends a local result with automatic 1-based index assignment.
    // Uses 1-based indexing because HashMap<size_t, ...> uses 0 as the empty value
    // (see AXCrossProcessSearchCoordinator::m_remoteResults).
    void appendLocalResult(Ref<AXCoreObject> object)
    {
        m_entries.append(SearchResultEntry::localResult(WTF::move(object), nextIndex()));
    }

    void appendRemoteFrame(FrameIdentifier frameID)
    {
        m_entries.append(SearchResultEntry::remoteFrame(frameID, nextIndex()));
    }

    const Vector<SearchResultEntry>& entries() const { return m_entries; }
    size_t entryCount() const { return m_entries.size(); }

    void setResultsLimit(unsigned limit) { m_resultsLimit = limit; }
    unsigned resultsLimit() const { return m_resultsLimit; }

private:
    size_t nextIndex() const { return m_entries.size() + 1; }

    Vector<SearchResultEntry> m_entries;
    unsigned m_resultsLimit { 0 };
};

// Represents a single search result that can be either a local accessibility object
// or a remote token from a cross-process search.
class AccessibilitySearchResult {
public:
    static AccessibilitySearchResult local(Ref<AXCoreObject> object)
    {
        return { object.ptr(), std::nullopt };
    }

    static AccessibilitySearchResult remote(AccessibilityRemoteToken&& token)
    {
        return { nullptr, WTF::move(token) };
    }

    bool isLocal() const { return m_localObject != nullptr; }
    bool isRemote() const { return m_remoteToken.has_value(); }

    RefPtr<AXCoreObject> objectIfLocalResult() const { return isLocal() ? m_localObject : nullptr; }
    const std::optional<AccessibilityRemoteToken>& remoteToken() const { return m_remoteToken; }

private:
    AccessibilitySearchResult(RefPtr<AXCoreObject> object, std::optional<AccessibilityRemoteToken> token)
        : m_localObject(WTF::move(object))
        , m_remoteToken(WTF::move(token))
    {
        // A result must be either local or remote, but not both, and not neither.
        AX_ASSERT((m_localObject != nullptr) != m_remoteToken.has_value());
    }

    // For local results - the accessibility object in this process.
    RefPtr<AXCoreObject> m_localObject;

    // For remote results - the token to create a platform remote element.
    std::optional<AccessibilityRemoteToken> m_remoteToken;
};

// Vector of search results that can contain both local and remote results in tree order.
using AccessibilitySearchResults = Vector<AccessibilitySearchResult>;

// Callback invoked when a remote frame is encountered during search.
// Parameters: frameID, streamIndex, localResultCountSoFar
// This allows callers to dispatch IPC eagerly while the local search continues.
using RemoteFrameSearchCallback = Function<void(FrameIdentifier, size_t, unsigned)>;

class AXSearchManager {
    WTF_DEPRECATED_MAKE_FAST_ALLOCATED_WITH_HEAP_IDENTIFIER(AXSearchManager, AXSearchManager);
public:
    // Primary search API - returns results that may include both local objects and remote tokens.
    // The optional callback is invoked when a remote frame is encountered, allowing eager IPC dispatch.
    // Callers should use performSearchWithCrossProcessCoordination() from AXCrossProcessSearch.h
    // for automatic cross-process coordination.
    WEBCORE_EXPORT AccessibilitySearchResultStream findMatchingObjectsAsStream(AccessibilitySearchCriteria&&, RemoteFrameSearchCallback&& = nullptr);

    std::optional<AXTextMarkerRange> findMatchingRange(AccessibilitySearchCriteria&&);

private:
    AccessibilitySearchResultStream findMatchingObjectsInternalAsStream(const AccessibilitySearchCriteria&, const RemoteFrameSearchCallback&);
    bool match(Ref<AXCoreObject>, const AccessibilitySearchCriteria&);
    bool matchText(Ref<AXCoreObject>, const String&);
    bool matchForSearchKeyAtIndex(Ref<AXCoreObject>, const AccessibilitySearchCriteria&, size_t);
    DidTimeout revealHiddenMatchWithTimeout(AXCoreObject&, Seconds);

    bool lastRevealAttemptTimedOut()
    {
        if (isMainThread())
            return false;
        return m_lastRevealAttemptTimedOut;
    }
    void setLastRevealAttemptTimedOut(bool newValue)
    {
        AX_ASSERT(!isMainThread());
        m_lastRevealAttemptTimedOut = newValue;
    }

    // Keeps the ranges of misspellings for each object.
    HashMap<AXID, Vector<AXTextMarkerRange>> m_misspellingRanges;

    // For certain types of searches, we may detect that an object matching the search is in a collapsed,
    // but revealable / expandable container. We try to do this reveal synchronously from the accessibility thread
    // to the main-thread, but with a timeout in case the main-thread is busy. If the main-thread is busy once,
    // we don't want to try to synchronously reveal collapsed content again.
    //
    // This must only be read and written from the accessibility thread.
    bool m_lastRevealAttemptTimedOut { false };
};

WTF::TextStream& operator<<(WTF::TextStream&, AccessibilitySearchKey);
WTF::TextStream& operator<<(WTF::TextStream&, const AccessibilitySearchCriteria&);

} // namespace WebCore
