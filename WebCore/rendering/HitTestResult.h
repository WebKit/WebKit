/*
 * Copyright (C) 2006 Apple Computer, Inc.
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

#include "IntPoint.h"
#include "IntRect.h"
#include "IntSize.h"
#include "TextDirection.h"
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
class IntRect;
class KURL;
class Node;
class Scrollbar;

class HitTestResult {
public:
    HitTestResult(const IntPoint&);
    // Pass a non-negative IntSize value as padding to perform a rect-based hit test.
    HitTestResult(const IntPoint& centerPoint, const IntSize& padding);
    HitTestResult(const HitTestResult&);
    ~HitTestResult();
    HitTestResult& operator=(const HitTestResult&);

    Node* innerNode() const { return m_innerNode.get(); }
    Node* innerNonSharedNode() const { return m_innerNonSharedNode.get(); }
    IntPoint point() const { return m_point; }
    IntPoint localPoint() const { return m_localPoint; }
    Element* URLElement() const { return m_innerURLElement.get(); }
    Scrollbar* scrollbar() const { return m_scrollbar.get(); }
    bool isOverWidget() const { return m_isOverWidget; }

    void setToNonShadowAncestor();

    void setInnerNode(Node*);
    void setInnerNonSharedNode(Node*);
    void setPoint(const IntPoint& p) { m_point = p; }
    void setLocalPoint(const IntPoint& p) { m_localPoint = p; }
    void setURLElement(Element*);
    void setScrollbar(Scrollbar*);
    void setIsOverWidget(bool b) { m_isOverWidget = b; }

    Frame* targetFrame() const;
    bool isSelected() const;
    String spellingToolTip(TextDirection&) const;
    String replacedString() const;
    String title(TextDirection&) const;
    String altDisplayString() const;
    String titleDisplayString() const;
    Image* image() const;
    IntRect imageRect() const;
    KURL absoluteImageURL() const;
    KURL absoluteMediaURL() const;
    KURL absoluteLinkURL() const;
    String textContent() const;
    bool isLiveLink() const;
    bool isContentEditable() const;
    void toggleMediaControlsDisplay() const;
    void toggleMediaLoopPlayback() const;
    void enterFullscreenForVideo() const;
    bool mediaControlsEnabled() const;
    bool mediaLoopEnabled() const;
    bool mediaPlaying() const;
    bool mediaSupportsFullscreen() const;
    void toggleMediaPlayState() const;
    bool mediaHasAudio() const;
    bool mediaIsVideo() const;
    bool mediaMuted() const;
    void toggleMediaMuteState() const;

    // Rect-based hit test related methods.
    bool isRectBasedTest() const { return m_isRectBased; }
    IntRect rectFromPoint(int x, int y) const;
    IntRect rectFromPoint(const IntPoint&) const;
    IntSize padding() const { return m_padding; }
    int paddingWidth() const { return m_padding.width(); }
    int paddingHeight() const { return m_padding.height(); }
    // Returns true if it is rect-based hit test and needs to continue until the rect is fully
    // enclosed by the boundaries of a node.
    bool addNodeToRectBasedTestResult(Node*, int x, int y, const IntRect& rect = IntRect());
    const ListHashSet<RefPtr<Node> >& rectBasedTestResult() const { return m_rectBasedTestResult; }
    void append(const HitTestResult&);

private:

#if ENABLE(VIDEO)
    HTMLMediaElement* mediaElement() const;
#endif

    RefPtr<Node> m_innerNode;
    RefPtr<Node> m_innerNonSharedNode;
    IntPoint m_point;
    IntPoint m_localPoint; // A point in the local coordinate space of m_innerNonSharedNode's renderer.  Allows us to efficiently
                           // determine where inside the renderer we hit on subsequent operations.
    RefPtr<Element> m_innerURLElement;
    RefPtr<Scrollbar> m_scrollbar;
    bool m_isOverWidget; // Returns true if we are over a widget (and not in the border/padding area of a RenderWidget for example).
    bool m_isRectBased;
    IntSize m_padding;
    ListHashSet<RefPtr<Node> > m_rectBasedTestResult;
};

inline IntRect HitTestResult::rectFromPoint(int x, int y) const
{
    return rectFromPoint(IntPoint(x, y));
}

// Formula:
// x = p.x() - padding.width()
// y = p.y() - padding.height()
// width = 2 * padding.width() + 1
// height = 2 * m_padding.height() + 1
inline IntRect HitTestResult::rectFromPoint(const IntPoint& point) const
{
    IntPoint realPoint(point);
    IntSize realPadding(m_padding);

    // Real IntPoint for the rect.
    realPadding.clampNegativeToZero();
    realPoint -= realPadding;

    // Real IntSize for the rect.
    realPadding.scale(2);
    realPadding += IntSize(1, 1);

    return IntRect(realPoint, realPadding);
}

String displayString(const String&, const Node*);

} // namespace WebCore

#endif // HitTestResult_h
