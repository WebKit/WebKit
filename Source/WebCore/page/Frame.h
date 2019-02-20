/*
 * Copyright (C) 1998, 1999 Torben Weis <weis@kde.org>
 *                     1999-2001 Lars Knoll <knoll@kde.org>
 *                     1999-2001 Antti Koivisto <koivisto@kde.org>
 *                     2000-2001 Simon Hausmann <hausmann@kde.org>
 *                     2000-2001 Dirk Mueller <mueller@kde.org>
 *                     2000 Stefan Schimanski <1Stein@gmx.de>
 * Copyright (C) 2004-2018 Apple Inc. All rights reserved.
 * Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2008 Eric Seidel <eric@webkit.org>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Library General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public License
 * along with this library; see the file COPYING.LIB.  If not, write to
 * the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 * Boston, MA 02110-1301, USA.
 */

#pragma once

#include "AbstractFrame.h"
#include "AdjustViewSizeOrNot.h"
#include "FrameTree.h"
#include "ScrollTypes.h"
#include "UserScriptTypes.h"
#include <wtf/HashSet.h>
#include <wtf/UniqueRef.h>

#if PLATFORM(IOS_FAMILY)
#include "Timer.h"
#include "ViewportArguments.h"
#include "VisibleSelection.h"
#endif

#if PLATFORM(WIN)
#include "FrameWin.h"
#endif

#if PLATFORM(COCOA)
OBJC_CLASS NSArray;
#endif

#if PLATFORM(WIN)
typedef struct HBITMAP__* HBITMAP;
#endif

namespace JSC { namespace Yarr {
class RegularExpression;
} }

namespace WebCore {

class CSSAnimationController;
class Color;
class DOMWindow;
class Document;
class Editor;
class Element;
class EventHandler;
class FloatSize;
class FrameDestructionObserver;
class FrameLoader;
class FrameLoaderClient;
class FrameSelection;
class FrameView;
class HTMLFrameOwnerElement;
class HTMLTableCellElement;
class HitTestResult;
class ImageBuffer;
class IntPoint;
class IntRect;
class IntSize;
class NavigationScheduler;
class Node;
class Page;
class Range;
class RenderLayer;
class RenderView;
class RenderWidget;
class ScriptController;
class SecurityOrigin;
class Settings;
class VisiblePosition;
class Widget;

#if PLATFORM(IOS_FAMILY)
enum {
    OverflowScrollNone = 0,
    OverflowScrollLeft = 1 << 0,
    OverflowScrollRight = 1 << 1,
    OverflowScrollUp = 1 << 2,
    OverflowScrollDown = 1 << 3
};

enum OverflowScrollAction { DoNotPerformOverflowScroll, PerformOverflowScroll };
using NodeQualifier = Function<Node* (const HitTestResult&, Node* terminationNode, IntRect* nodeBounds)>;
#endif

enum {
    LayerTreeFlagsIncludeDebugInfo = 1 << 0,
    LayerTreeFlagsIncludeVisibleRects = 1 << 1,
    LayerTreeFlagsIncludeTileCaches = 1 << 2,
    LayerTreeFlagsIncludeRepaintRects = 1 << 3,
    LayerTreeFlagsIncludePaintingPhases = 1 << 4,
    LayerTreeFlagsIncludeContentLayers = 1 << 5,
    LayerTreeFlagsIncludeAcceleratesDrawing = 1 << 6,
    LayerTreeFlagsIncludeBackingStoreAttached = 1 << 7,
    LayerTreeFlagsIncludeRootLayerProperties = 1 << 8,
};
typedef unsigned LayerTreeFlags;

// FIXME: Rename Frame to LocalFrame and AbstractFrame to Frame.
class Frame final : public AbstractFrame {
public:
    WEBCORE_EXPORT static Ref<Frame> create(Page*, HTMLFrameOwnerElement*, FrameLoaderClient*);

    WEBCORE_EXPORT void init();
#if PLATFORM(IOS_FAMILY)
    // Creates <html><body style="..."></body></html> doing minimal amount of work.
    WEBCORE_EXPORT void initWithSimpleHTMLDocument(const String& style, const URL&);
#endif
    WEBCORE_EXPORT void setView(RefPtr<FrameView>&&);
    WEBCORE_EXPORT void createView(const IntSize&, bool transparent,
        const IntSize& fixedLayoutSize, const IntRect& fixedVisibleContentRect,
        bool useFixedLayout = false, ScrollbarMode = ScrollbarAuto, bool horizontalLock = false,
        ScrollbarMode = ScrollbarAuto, bool verticalLock = false);

    WEBCORE_EXPORT ~Frame();

    WEBCORE_EXPORT DOMWindow* window() const;

    void addDestructionObserver(FrameDestructionObserver*);
    void removeDestructionObserver(FrameDestructionObserver*);

    WEBCORE_EXPORT void willDetachPage();
    void detachFromPage();
    void disconnectOwnerElement();

    Frame& mainFrame() const;
    bool isMainFrame() const { return this == static_cast<void*>(&m_mainFrame); }

    Page* page() const;
    HTMLFrameOwnerElement* ownerElement() const;

    Document* document() const;
    FrameView* view() const;

    Editor& editor() { return m_editor; }
    const Editor& editor() const { return m_editor; }
    EventHandler& eventHandler() { return m_eventHandler; }
    const EventHandler& eventHandler() const { return m_eventHandler; }
    FrameLoader& loader() const;
    NavigationScheduler& navigationScheduler() const;
    FrameSelection& selection() { return m_selection; }
    const FrameSelection& selection() const { return m_selection; }
    FrameTree& tree() const;
    CSSAnimationController& animation() { return m_animationController; }
    const CSSAnimationController& animation() const { return m_animationController; }
    ScriptController& script() { return m_script; }
    const ScriptController& script() const { return m_script; }
    
    WEBCORE_EXPORT RenderView* contentRenderer() const; // Root of the render tree for the document contained in this frame.
    WEBCORE_EXPORT RenderWidget* ownerRenderer() const; // Renderer for the element that contains this frame.

    bool documentIsBeingReplaced() const { return m_documentIsBeingReplaced; }

    bool hasHadUserInteraction() const { return m_hasHadUserInteraction; }
    void setHasHadUserInteraction() { m_hasHadUserInteraction = true; }

    bool requestDOMPasteAccess();

// ======== All public functions below this point are candidates to move out of Frame into another class. ========

    WEBCORE_EXPORT void injectUserScripts(UserScriptInjectionTime);
    WEBCORE_EXPORT void injectUserScriptImmediately(DOMWrapperWorld&, const UserScript&);
    
    WEBCORE_EXPORT String layerTreeAsText(LayerTreeFlags = 0) const;
    WEBCORE_EXPORT String trackedRepaintRectsAsText() const;

    WEBCORE_EXPORT static Frame* frameForWidget(const Widget&);

    Settings& settings() const { return *m_settings; }

    void setPrinting(bool printing, const FloatSize& pageSize, const FloatSize& originalPageSize, float maximumShrinkRatio, AdjustViewSizeOrNot);
    bool shouldUsePrintingLayout() const;
    WEBCORE_EXPORT FloatSize resizePageRectsKeepingRatio(const FloatSize& originalSize, const FloatSize& expectedSize);

    void setDocument(RefPtr<Document>&&);

    WEBCORE_EXPORT void setPageZoomFactor(float);
    float pageZoomFactor() const { return m_pageZoomFactor; }
    WEBCORE_EXPORT void setTextZoomFactor(float);
    float textZoomFactor() const { return m_textZoomFactor; }
    WEBCORE_EXPORT void setPageAndTextZoomFactors(float pageZoomFactor, float textZoomFactor);

    // Scale factor of this frame with respect to the container.
    WEBCORE_EXPORT float frameScaleFactor() const;

    void deviceOrPageScaleFactorChanged();
    
#if ENABLE(DATA_DETECTION)
    void setDataDetectionResults(NSArray *results) { m_dataDetectionResults = results; }
    NSArray *dataDetectionResults() const { return m_dataDetectionResults.get(); }
#endif

#if PLATFORM(IOS_FAMILY)
    const ViewportArguments& viewportArguments() const;
    WEBCORE_EXPORT void setViewportArguments(const ViewportArguments&);

    WEBCORE_EXPORT Node* deepestNodeAtLocation(const FloatPoint& viewportLocation);
    WEBCORE_EXPORT Node* nodeRespondingToClickEvents(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation, SecurityOrigin* = nullptr);
    WEBCORE_EXPORT Node* nodeRespondingToScrollWheelEvents(const FloatPoint& viewportLocation);

    WEBCORE_EXPORT NSArray *wordsInCurrentParagraph() const;
    WEBCORE_EXPORT CGRect renderRectForPoint(CGPoint, bool* isReplaced, float* fontSize) const;

    WEBCORE_EXPORT void setSelectionChangeCallbacksDisabled(bool = true);
    bool selectionChangeCallbacksDisabled() const;

    enum ViewportOffsetChangeType { IncrementalScrollOffset, CompletedScrollOffset };
    WEBCORE_EXPORT void viewportOffsetChanged(ViewportOffsetChangeType);
    bool containsTiledBackingLayers() const;

    WEBCORE_EXPORT void overflowScrollPositionChangedForNode(const IntPoint&, Node*, bool isUserScroll);

    WEBCORE_EXPORT void resetAllGeolocationPermission();
#endif

#if ENABLE(ORIENTATION_EVENTS)
    // Orientation is the interface orientation in degrees. Some examples are:
    //  0 is straight up; -90 is when the device is rotated 90 clockwise;
    //  90 is when rotated counter clockwise.
    WEBCORE_EXPORT void orientationChanged();
    int orientation() const;
#endif

    void clearTimers();
    static void clearTimers(FrameView*, Document*);

    WEBCORE_EXPORT String displayStringModifiedByEncoding(const String&) const;

    WEBCORE_EXPORT VisiblePosition visiblePositionForPoint(const IntPoint& framePoint) const;
    Document* documentAtPoint(const IntPoint& windowPoint);
    WEBCORE_EXPORT RefPtr<Range> rangeForPoint(const IntPoint& framePoint);

    WEBCORE_EXPORT String searchForLabelsAboveCell(const JSC::Yarr::RegularExpression&, HTMLTableCellElement*, size_t* resultDistanceFromStartOfCell);
    String searchForLabelsBeforeElement(const Vector<String>& labels, Element*, size_t* resultDistance, bool* resultIsInCellAbove);
    String matchLabelsAgainstElement(const Vector<String>& labels, Element*);

#if PLATFORM(IOS_FAMILY)
    // Scroll the selection in an overflow layer.
    void scrollOverflowLayer(RenderLayer*, const IntRect& visibleRect, const IntRect& exposeRect);

    WEBCORE_EXPORT int preferredHeight() const;
    WEBCORE_EXPORT void updateLayout() const;
    WEBCORE_EXPORT NSRect caretRect();
    WEBCORE_EXPORT NSRect rectForScrollToVisible();
    WEBCORE_EXPORT unsigned formElementsCharacterCount() const;

    // This function is used by Legacy WebKit.
    WEBCORE_EXPORT void setTimersPaused(bool);

    WEBCORE_EXPORT void dispatchPageHideEventBeforePause();
    WEBCORE_EXPORT void dispatchPageShowEventBeforeResume();
    WEBCORE_EXPORT void setRangedSelectionBaseToCurrentSelection();
    WEBCORE_EXPORT void setRangedSelectionBaseToCurrentSelectionStart();
    WEBCORE_EXPORT void setRangedSelectionBaseToCurrentSelectionEnd();
    WEBCORE_EXPORT void clearRangedSelectionInitialExtent();
    WEBCORE_EXPORT void setRangedSelectionInitialExtentToCurrentSelectionStart();
    WEBCORE_EXPORT void setRangedSelectionInitialExtentToCurrentSelectionEnd();
    WEBCORE_EXPORT VisibleSelection rangedSelectionBase() const;
    WEBCORE_EXPORT VisibleSelection rangedSelectionInitialExtent() const;
    WEBCORE_EXPORT void recursiveSetUpdateAppearanceEnabled(bool);
    WEBCORE_EXPORT NSArray *interpretationsForCurrentRoot() const;
#endif
    void suspendActiveDOMObjectsAndAnimations();
    void resumeActiveDOMObjectsAndAnimations();
    bool activeDOMObjectsAndAnimationsSuspended() const { return m_activeDOMObjectsAndAnimationsSuspendedCount > 0; }

    bool isURLAllowed(const URL&) const;
    WEBCORE_EXPORT bool isAlwaysOnLoggingAllowed() const;

// ========

    void selfOnlyRef();
    void selfOnlyDeref();

private:
    friend class NavigationDisabler;

    Frame(Page&, HTMLFrameOwnerElement*, FrameLoaderClient&);

    void dropChildren();

    bool isLocalFrame() const final { return true; }
    bool isRemoteFrame() const final { return false; }

    AbstractDOMWindow* virtualWindow() const final;

    HashSet<FrameDestructionObserver*> m_destructionObservers;

    Frame& m_mainFrame;
    Page* m_page;
    const RefPtr<Settings> m_settings;
    mutable FrameTree m_treeNode;
    mutable UniqueRef<FrameLoader> m_loader;
    mutable UniqueRef<NavigationScheduler> m_navigationScheduler;

    HTMLFrameOwnerElement* m_ownerElement;
    RefPtr<FrameView> m_view;
    RefPtr<Document> m_doc;

    UniqueRef<ScriptController> m_script;
    UniqueRef<Editor> m_editor;
    UniqueRef<FrameSelection> m_selection;
    UniqueRef<CSSAnimationController> m_animationController;

#if ENABLE(DATA_DETECTION)
    RetainPtr<NSArray> m_dataDetectionResults;
#endif
#if PLATFORM(IOS_FAMILY)
    void betterApproximateNode(const IntPoint& testPoint, const NodeQualifier&, Node*& best, Node* failedNode, IntPoint& bestPoint, IntRect& bestRect, const IntRect& testRect);
    bool hitTestResultAtViewportLocation(const FloatPoint& viewportLocation, HitTestResult&, IntPoint& center);
    Node* qualifyingNodeAtViewportLocation(const FloatPoint& viewportLocation, FloatPoint& adjustedViewportLocation, const NodeQualifier&, bool shouldApproximate);

    void overflowAutoScrollTimerFired();
    void startOverflowAutoScroll(const IntPoint&);
    int checkOverflowScroll(OverflowScrollAction);

    void setTimersPausedInternal(bool);

    Timer m_overflowAutoScrollTimer;
    float m_overflowAutoScrollDelta;
    IntPoint m_overflowAutoScrollPos;
    ViewportArguments m_viewportArguments;
    bool m_selectionChangeCallbacksDisabled;
    VisibleSelection m_rangedSelectionBase;
    VisibleSelection m_rangedSelectionInitialExtent;
#endif

    float m_pageZoomFactor;
    float m_textZoomFactor;

    int m_activeDOMObjectsAndAnimationsSuspendedCount;
    bool m_documentIsBeingReplaced { false };
    unsigned m_navigationDisableCount { 0 };
    unsigned m_selfOnlyRefCount { 0 };
    bool m_hasHadUserInteraction { false };

protected:
    UniqueRef<EventHandler> m_eventHandler;
};

inline FrameLoader& Frame::loader() const
{
    return m_loader.get();
}

inline NavigationScheduler& Frame::navigationScheduler() const
{
    return m_navigationScheduler.get();
}

inline FrameView* Frame::view() const
{
    return m_view.get();
}

inline Document* Frame::document() const
{
    return m_doc.get();
}

inline HTMLFrameOwnerElement* Frame::ownerElement() const
{
    return m_ownerElement;
}

inline FrameTree& Frame::tree() const
{
    return m_treeNode;
}

inline Page* Frame::page() const
{
    return m_page;
}

inline void Frame::detachFromPage()
{
    m_page = nullptr;
}

inline Frame& Frame::mainFrame() const
{
    return m_mainFrame;
}

} // namespace WebCore

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::Frame)
    static bool isType(const WebCore::AbstractFrame& frame) { return frame.isLocalFrame(); }
SPECIALIZE_TYPE_TRAITS_END()
