/*
 * Copyright (C) 2006-2017 Apple Inc. All rights reserved.
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

#pragma once

#include "ActivityState.h"
#include "FocusOptions.h"
#include "LayoutRect.h"
#include "LocalFrame.h"
#include "Timer.h"
#include <wtf/CheckedRef.h>
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ContainerNode;
class Document;
class Element;
class FocusNavigationScope;
class Frame;
class IntRect;
class KeyboardEvent;
class Node;
class Page;
class TreeScope;

struct FocusCandidate;
struct FocusEventData;

enum class ContinuedSearchInRemoteFrame : bool { No, Yes };
enum class FoundElementInRemoteFrame : bool { No, Yes };

struct FocusableElementSearchResult {
    RefPtr<Element> element;
    ContinuedSearchInRemoteFrame continuedSearchInRemoteFrame { ContinuedSearchInRemoteFrame::No };
};

class FocusController final : public CanMakeCheckedPtr<FocusController> {
    WTF_MAKE_TZONE_ALLOCATED(FocusController);
    WTF_OVERRIDE_DELETE_FOR_CHECKED_PTR(FocusController);
public:
    explicit FocusController(Page&, OptionSet<ActivityState>);

    enum class BroadcastFocusedFrame : bool { No, Yes };
    WEBCORE_EXPORT void setFocusedFrame(Frame*, BroadcastFocusedFrame = BroadcastFocusedFrame::Yes);
    Frame* focusedFrame() const { return m_focusedFrame.get(); }
    LocalFrame* focusedLocalFrame() const { return dynamicDowncast<LocalFrame>(m_focusedFrame.get()); }
    WEBCORE_EXPORT LocalFrame* focusedOrMainFrame() const;
    RefPtr<LocalFrame> protectedFocusedOrMainFrame() const { return focusedOrMainFrame(); }

    WEBCORE_EXPORT bool setInitialFocus(FocusDirection, KeyboardEvent*);
    bool advanceFocus(FocusDirection, KeyboardEvent*, bool initialFocus = false);

    WEBCORE_EXPORT bool setFocusedElement(Element*, LocalFrame&, const FocusOptions& = { });

    void setActivityState(OptionSet<ActivityState>);

    WEBCORE_EXPORT void setActive(bool);
    bool isActive() const { return m_activityState.contains(ActivityState::WindowIsActive); }

    WEBCORE_EXPORT void setFocused(bool);
    bool isFocused() const { return m_activityState.contains(ActivityState::IsFocused); }

    bool contentIsVisible() const { return m_activityState.contains(ActivityState::IsVisible); }

    WEBCORE_EXPORT FocusableElementSearchResult nextFocusableElement(Node&);
    WEBCORE_EXPORT FocusableElementSearchResult previousFocusableElement(Node&);

    void setFocusedElementNeedsRepaint();
    Seconds timeSinceFocusWasSet() const;

    WEBCORE_EXPORT bool relinquishFocusToChrome(FocusDirection);

    WEBCORE_EXPORT FocusableElementSearchResult findAndFocusElementStartingWithLocalFrame(FocusDirection, const FocusEventData&, LocalFrame&);

private:
    void setActiveInternal(bool);
    void setFocusedInternal(bool);
    void setIsVisibleAndActiveInternal(bool);

    enum class InitialFocus : bool { No, Yes };
    enum class ContinuingRemoteSearch : bool { No, Yes };

    bool advanceFocusDirectionally(FocusDirection, const FocusEventData&);
    bool advanceFocusInDocumentOrder(FocusDirection, const FocusEventData&, InitialFocus);

    FocusableElementSearchResult findAndFocusElementInDocumentOrderStartingWithFrame(Ref<LocalFrame>, RefPtr<Node> scopeNode, RefPtr<Node> startingNode, FocusDirection, const FocusEventData&, InitialFocus, ContinuingRemoteSearch);

    FocusableElementSearchResult findFocusableElementAcrossFocusScope(FocusDirection, const FocusNavigationScope& startScope, Node* start, const FocusEventData&);

    FocusableElementSearchResult findFocusableElementWithinScope(FocusDirection, const FocusNavigationScope&, Node* start, const FocusEventData&);
    FocusableElementSearchResult nextFocusableElementWithinScope(const FocusNavigationScope&, Node* start, const FocusEventData&);
    FocusableElementSearchResult previousFocusableElementWithinScope(const FocusNavigationScope&, Node* start, const FocusEventData&);

    FocusableElementSearchResult findFocusableElementDescendingIntoSubframes(FocusDirection, Element*, const FocusEventData&);

    // Searches through the given tree scope, starting from start node, for the next/previous selectable element that comes after/before start node.
    // The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab indexes
    // first (from lowest to highest), and then elements without tab indexes (in document order).
    //
    // @param start The node from which to start searching. The node after this will be focused. May be null.
    //
    // @return The focus node that comes after/before start node.
    //
    // See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
    Element* findFocusableElementOrScopeOwner(FocusDirection, const FocusNavigationScope&, Node* start, const FocusEventData&);

    Element* findElementWithExactTabIndex(const FocusNavigationScope&, Node* start, int tabIndex, const FocusEventData&, FocusDirection);
    
    Element* nextFocusableElementOrScopeOwner(const FocusNavigationScope&, Node* start, const FocusEventData&);
    Element* previousFocusableElementOrScopeOwner(const FocusNavigationScope&, Node* start, const FocusEventData&);

    bool advanceFocusDirectionallyInContainer(const ContainerNode&, const LayoutRect& startingRect, FocusDirection, const FocusEventData&);
    void findFocusCandidateInContainer(const ContainerNode&, const LayoutRect& startingRect, FocusDirection, const FocusEventData&, FocusCandidate& closest);

    void focusRepaintTimerFired();
    Ref<Page> protectedPage() const;

    WeakRef<Page> m_page;
    WeakPtr<Frame> m_focusedFrame;
    bool m_isChangingFocusedFrame;
    OptionSet<ActivityState> m_activityState;

    Timer m_focusRepaintTimer;
    MonotonicTime m_focusSetTime;
};

} // namespace WebCore
