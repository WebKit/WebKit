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
#include "FocusDirection.h"
#include "LayoutRect.h"
#include "Timer.h"
#include <wtf/Forward.h>
#include <wtf/RefPtr.h>

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

class FocusController {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit FocusController(Page&, OptionSet<ActivityState::Flag>);

    WEBCORE_EXPORT void setFocusedFrame(Frame*);
    Frame* focusedFrame() const { return m_focusedFrame.get(); }
    WEBCORE_EXPORT Frame& focusedOrMainFrame() const;

    WEBCORE_EXPORT bool setInitialFocus(FocusDirection, KeyboardEvent*);
    bool advanceFocus(FocusDirection, KeyboardEvent*, bool initialFocus = false);

    WEBCORE_EXPORT bool setFocusedElement(Element*, Frame&, FocusDirection = FocusDirectionNone);

    void setActivityState(OptionSet<ActivityState::Flag>);

    WEBCORE_EXPORT void setActive(bool);
    bool isActive() const { return m_activityState.contains(ActivityState::WindowIsActive); }

    WEBCORE_EXPORT void setFocused(bool);
    bool isFocused() const { return m_activityState.contains(ActivityState::IsFocused); }

    bool contentIsVisible() const { return m_activityState.contains(ActivityState::IsVisible); }

    // These methods are used in WebCore/bindings/objc/DOM.mm.
    WEBCORE_EXPORT Element* nextFocusableElement(Node&);
    WEBCORE_EXPORT Element* previousFocusableElement(Node&);

    void setFocusedElementNeedsRepaint();
    Seconds timeSinceFocusWasSet() const;

    bool relinquishFocusToChrome(FocusDirection);

private:
    void setActiveInternal(bool);
    void setFocusedInternal(bool);
    void setIsVisibleAndActiveInternal(bool);

    bool advanceFocusDirectionally(FocusDirection, KeyboardEvent*);
    bool advanceFocusInDocumentOrder(FocusDirection, KeyboardEvent*, bool initialFocus);

    Element* findFocusableElementAcrossFocusScope(FocusDirection, const FocusNavigationScope& startScope, Node* start, KeyboardEvent*);

    Element* findFocusableElementWithinScope(FocusDirection, const FocusNavigationScope&, Node* start, KeyboardEvent*);
    Element* nextFocusableElementWithinScope(const FocusNavigationScope&, Node* start, KeyboardEvent*);
    Element* previousFocusableElementWithinScope(const FocusNavigationScope&, Node* start, KeyboardEvent*);

    Element* findFocusableElementDescendingIntoSubframes(FocusDirection, Element*, KeyboardEvent*);

    // Searches through the given tree scope, starting from start node, for the next/previous selectable element that comes after/before start node.
    // The order followed is as specified in section 17.11.1 of the HTML4 spec, which is elements with tab indexes
    // first (from lowest to highest), and then elements without tab indexes (in document order).
    //
    // @param start The node from which to start searching. The node after this will be focused. May be null.
    //
    // @return The focus node that comes after/before start node.
    //
    // See http://www.w3.org/TR/html4/interact/forms.html#h-17.11.1
    Element* findFocusableElementOrScopeOwner(FocusDirection, const FocusNavigationScope&, Node* start, KeyboardEvent*);

    Element* findElementWithExactTabIndex(const FocusNavigationScope&, Node* start, int tabIndex, KeyboardEvent*, FocusDirection);
    
    Element* nextFocusableElementOrScopeOwner(const FocusNavigationScope&, Node* start, KeyboardEvent*);
    Element* previousFocusableElementOrScopeOwner(const FocusNavigationScope&, Node* start, KeyboardEvent*);

    bool advanceFocusDirectionallyInContainer(Node* container, const LayoutRect& startingRect, FocusDirection, KeyboardEvent*);
    void findFocusCandidateInContainer(Node& container, const LayoutRect& startingRect, FocusDirection, KeyboardEvent*, FocusCandidate& closest);

    void focusRepaintTimerFired();

    Page& m_page;
    RefPtr<Frame> m_focusedFrame;
    bool m_isChangingFocusedFrame;
    OptionSet<ActivityState::Flag> m_activityState;

    Timer m_focusRepaintTimer;
    MonotonicTime m_focusSetTime;
};

} // namespace WebCore
