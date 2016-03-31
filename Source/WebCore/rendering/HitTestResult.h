/*
 * Copyright (C) 2006 Apple Inc.
 * Copyright (C) 2012 Nokia Corporation and/or its subsidiary(-ies)
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
 *
*/

#ifndef HitTestResult_h
#define HitTestResult_h

#include "FloatQuad.h"
#include "FloatRect.h"
#include "HitTestLocation.h"
#include "HitTestRequest.h"
#include "LayoutRect.h"
#include "TextFlags.h"
#include <memory>
#include <wtf/Forward.h>
#include <wtf/ListHashSet.h>
#include <wtf/RefPtr.h>

namespace WebCore {

class Element;
class Frame;
#if ENABLE(VIDEO)
class HTMLMediaElement;
#endif
class Image;
class URL;
class Node;
class Scrollbar;

class HitTestResult {
public:
    typedef ListHashSet<RefPtr<Node>> NodeSet;

    WEBCORE_EXPORT HitTestResult();
    WEBCORE_EXPORT explicit HitTestResult(const LayoutPoint&);
    // Pass non-negative padding values to perform a rect-based hit test.
    WEBCORE_EXPORT HitTestResult(const LayoutPoint& centerPoint, unsigned topPadding, unsigned rightPadding, unsigned bottomPadding, unsigned leftPadding);
    WEBCORE_EXPORT explicit HitTestResult(const HitTestLocation&);
    WEBCORE_EXPORT HitTestResult(const HitTestResult&);
    WEBCORE_EXPORT ~HitTestResult();
    WEBCORE_EXPORT HitTestResult& operator=(const HitTestResult&);

    Node* innerNode() const { return m_innerNode.get(); }
    WEBCORE_EXPORT Element* innerElement() const;
    Node* innerNonSharedNode() const { return m_innerNonSharedNode.get(); }
    WEBCORE_EXPORT Element* innerNonSharedElement() const;
    Element* URLElement() const { return m_innerURLElement.get(); }
    Scrollbar* scrollbar() const { return m_scrollbar.get(); }
    bool isOverWidget() const { return m_isOverWidget; }

    // Forwarded from HitTestLocation
    bool isRectBasedTest() const { return m_hitTestLocation.isRectBasedTest(); }

    // The hit-tested point in the coordinates of the main frame.
    const LayoutPoint& pointInMainFrame() const { return m_hitTestLocation.point(); }
    IntPoint roundedPointInMainFrame() const { return roundedIntPoint(pointInMainFrame()); }

    // The hit-tested point in the coordinates of the innerNode frame, the frame containing innerNode.
    const LayoutPoint& pointInInnerNodeFrame() const { return m_pointInInnerNodeFrame; }
    IntPoint roundedPointInInnerNodeFrame() const { return roundedIntPoint(pointInInnerNodeFrame()); }
    WEBCORE_EXPORT Frame* innerNodeFrame() const;

    // The hit-tested point in the coordinates of the inner node.
    const LayoutPoint& localPoint() const { return m_localPoint; }
    void setLocalPoint(const LayoutPoint& p) { m_localPoint = p; }

    WEBCORE_EXPORT void setToNonShadowAncestor();

    const HitTestLocation& hitTestLocation() const { return m_hitTestLocation; }

    WEBCORE_EXPORT void setInnerNode(Node*);
    void setInnerNonSharedNode(Node*);
    void setURLElement(Element*);
    void setScrollbar(Scrollbar*);
    void setIsOverWidget(bool b) { m_isOverWidget = b; }

    WEBCORE_EXPORT Frame* targetFrame() const;
    WEBCORE_EXPORT bool isSelected() const;
    WEBCORE_EXPORT String selectedText() const;
    WEBCORE_EXPORT String spellingToolTip(TextDirection&) const;
    String replacedString() const;
    WEBCORE_EXPORT String title(TextDirection&) const;
    String innerTextIfTruncated(TextDirection&) const;
    WEBCORE_EXPORT String altDisplayString() const;
    WEBCORE_EXPORT String titleDisplayString() const;
    WEBCORE_EXPORT Image* image() const;
    WEBCORE_EXPORT IntRect imageRect() const;
    WEBCORE_EXPORT URL absoluteImageURL() const;
    WEBCORE_EXPORT URL absolutePDFURL() const;
    WEBCORE_EXPORT URL absoluteMediaURL() const;
    WEBCORE_EXPORT URL absoluteLinkURL() const;
#if ENABLE(ATTACHMENT_ELEMENT)
    WEBCORE_EXPORT URL absoluteAttachmentURL() const;
#endif
    WEBCORE_EXPORT String textContent() const;
    bool isOverLink() const;
    WEBCORE_EXPORT bool isContentEditable() const;
    void toggleMediaControlsDisplay() const;
    void toggleMediaLoopPlayback() const;
    WEBCORE_EXPORT bool mediaIsInFullscreen() const;
    void toggleMediaFullscreenState() const;
    void enterFullscreenForVideo() const;
    bool mediaControlsEnabled() const;
    bool mediaLoopEnabled() const;
    bool mediaPlaying() const;
    bool mediaSupportsFullscreen() const;
    void toggleMediaPlayState() const;
    WEBCORE_EXPORT bool mediaHasAudio() const;
    WEBCORE_EXPORT bool mediaIsVideo() const;
    bool mediaMuted() const;
    void toggleMediaMuteState() const;
    bool mediaSupportsEnhancedFullscreen() const;
    bool mediaIsInEnhancedFullscreen() const;
    void toggleEnhancedFullscreenForVideo() const;

    WEBCORE_EXPORT bool isDownloadableMedia() const;
    WEBCORE_EXPORT bool isOverTextInsideFormControlElement() const;
    WEBCORE_EXPORT bool allowsCopy() const;

    // Returns true if it is rect-based hit test and needs to continue until the rect is fully
    // enclosed by the boundaries of a node.
    bool addNodeToRectBasedTestResult(Node*, const HitTestRequest&, const HitTestLocation& pointInContainer, const LayoutRect& = LayoutRect());
    bool addNodeToRectBasedTestResult(Node*, const HitTestRequest&, const HitTestLocation& pointInContainer, const FloatRect&);
    void append(const HitTestResult&);

    // If m_rectBasedTestResult is 0 then set it to a new NodeSet. Return *m_rectBasedTestResult. Lazy allocation makes
    // sense because the NodeSet is seldom necessary, and it's somewhat expensive to allocate and initialize. This method does
    // the same thing as mutableRectBasedTestResult(), but here the return value is const.
    WEBCORE_EXPORT const NodeSet& rectBasedTestResult() const;

    Vector<String> dictationAlternatives() const;

    Node* targetNode() const;

private:
    NodeSet& mutableRectBasedTestResult(); // See above.

#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement() const;
#endif
    HitTestLocation m_hitTestLocation;

    RefPtr<Node> m_innerNode;
    RefPtr<Node> m_innerNonSharedNode;
    LayoutPoint m_pointInInnerNodeFrame; // The hit-tested point in innerNode frame coordinates.
    LayoutPoint m_localPoint; // A point in the local coordinate space of m_innerNonSharedNode's renderer. Allows us to efficiently
                              // determine where inside the renderer we hit on subsequent operations.
    RefPtr<Element> m_innerURLElement;
    RefPtr<Scrollbar> m_scrollbar;
    bool m_isOverWidget; // Returns true if we are over a widget (and not in the border/padding area of a RenderWidget for example).

    mutable std::unique_ptr<NodeSet> m_rectBasedTestResult;
};

String displayString(const String&, const Node*);

} // namespace WebCore

#endif // HitTestResult_h
