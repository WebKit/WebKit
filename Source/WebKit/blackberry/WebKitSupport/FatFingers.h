/*
 * Copyright (C) 2010, 2011, 2012 Research In Motion Limited. All rights reserved.
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef FatFingers_h
#define FatFingers_h

#include "HitTestResult.h"
#include "RenderLayer.h"

#include <BlackBerryPlatformIntRectRegion.h>

#include <utility>

#include <wtf/HashSet.h>
#include <wtf/ListHashSet.h>
#include <wtf/Vector.h>

namespace WebCore {
class Document;
class Element;
class IntPoint;
class IntRect;
class IntSize;
class Node;
}

#define DEBUG_FAT_FINGERS 0

namespace BlackBerry {
namespace WebKit {

class WebPagePrivate;
class FatFingers;
class TouchEventHandler;

class FatFingersResult {
public:

    FatFingersResult(const WebCore::IntPoint& p = WebCore::IntPoint::zero())
        : m_originalPosition(p)
        , m_adjustedPosition(p)
        , m_positionWasAdjusted(false)
        , m_isTextInput(false)
        , m_isValid(false)
        , m_nodeUnderFatFinger(0)
    {
    }

    void reset()
    {
        m_originalPosition = m_adjustedPosition = WebCore::IntPoint::zero();
        m_positionWasAdjusted = false;
        m_isTextInput = false;
        m_isValid = false;
        m_nodeUnderFatFinger = 0;
    }

    WebCore::IntPoint originPosition() const { return m_originalPosition; }
    WebCore::IntPoint adjustedPosition() const { return m_adjustedPosition; }
    bool positionWasAdjusted() const { return m_isValid && m_positionWasAdjusted; }
    bool isTextInput() const { return m_isValid && !!m_nodeUnderFatFinger && m_isTextInput; }
    bool isValid() const { return m_isValid; }

    enum ContentType { ShadowContentAllowed, ShadowContentNotAllowed };

    WebCore::Node* node(ContentType type = ShadowContentAllowed) const
    {
        if (!m_nodeUnderFatFinger || !m_nodeUnderFatFinger->inDocument())
            return 0;

        WebCore::Node* result = m_nodeUnderFatFinger.get();

        if (type == ShadowContentAllowed)
            return result;

        // Shadow trees can be nested.
        while (result->isInShadowTree())
            result = toElement(result->shadowAncestorNode());

        return result;
    }

    WebCore::Element* nodeAsElementIfApplicable(ContentType type = ShadowContentAllowed) const
    {
        WebCore::Node* result = node(type);
        if (!result || !result->isElementNode())
            return 0;

        return static_cast<WebCore::Element*>(result);
    }

private:
    friend class WebKit::FatFingers;
    friend class WebKit::TouchEventHandler;

    WebCore::IntPoint m_originalPosition; // Main frame contents coordinates.
    WebCore::IntPoint m_adjustedPosition; // Main frame contents coordinates.
    bool m_positionWasAdjusted;
    bool m_isTextInput; // Check if the element under the touch point will require a VKB be displayed so that
                        // the touch down can be suppressed.
    bool m_isValid;
    RefPtr<WebCore::Node> m_nodeUnderFatFinger;
};

class FatFingers {
public:
    enum TargetType { ClickableElement, Text };

    FatFingers(WebPagePrivate* webpage, const WebCore::IntPoint& contentPos, TargetType);
    ~FatFingers();

    const FatFingersResult findBestPoint();

#if DEBUG_FAT_FINGERS
    // These debug vars are all in content coordinates. They are public so
    // they can be read from BackingStore, which will draw a visible rect
    // around the fat fingers area.
    static WebCore::IntRect m_debugFatFingerRect;
    static WebCore::IntPoint m_debugFatFingerClickPosition;
    static WebCore::IntPoint m_debugFatFingerAdjustedPosition;
#endif

private:
    enum MatchingApproachForClickable { ClickableByDefault = 0, MadeClickableByTheWebpage, Done };

    typedef std::pair<WebCore::Node*, Platform::IntRectRegion> IntersectingRegion;

    enum CachedResultsStrategy { GetFromRenderTree = 0, GetFromCache };
    CachedResultsStrategy cachingStrategy() const;
    typedef HashMap<RefPtr<WebCore::Document>, ListHashSet<RefPtr<WebCore::Node> > > CachedRectHitTestResults;

    bool checkFingerIntersection(const Platform::IntRectRegion&,
                                 const Platform::IntRectRegion& remainingFingerRegion,
                                 WebCore::Node*,
                                 Vector<IntersectingRegion>& intersectingRegions);

    bool findIntersectingRegions(WebCore::Document*,
                                 Vector<IntersectingRegion>& intersectingRegions,
                                 Platform::IntRectRegion& remainingFingerRegion);

    bool checkForClickableElement(WebCore::Element*,
                                  Vector<IntersectingRegion>& intersectingRegions,
                                  Platform::IntRectRegion& remainingFingerRegion,
                                  WebCore::RenderLayer*& lowestPositionedEnclosingLayerSoFar);

    bool checkForText(WebCore::Node*,
                      Vector<IntersectingRegion>& intersectingRegions,
                      Platform::IntRectRegion& fingerRegion);

    void setSuccessfulFatFingersResult(FatFingersResult&, WebCore::Node*, const WebCore::IntPoint&);

    void getNodesFromRect(WebCore::Document*, const WebCore::IntPoint&, ListHashSet<RefPtr<WebCore::Node> >&);

    // It mimics Document::elementFromPoint, but recursively hit-tests in case an inner frame is found.
    void getRelevantInfoFromPoint(WebCore::Document*,
                                  const WebCore::IntPoint&,
                                  WebCore::Element*& elementUnderPoint,
                                  WebCore::Element*& clickableElementUnderPoint) const;

    bool isElementClickable(WebCore::Element*) const;

    inline WebCore::IntRect fingerRectForPoint(const WebCore::IntPoint&) const;
    void getPaddings(unsigned& top, unsigned& right, unsigned& bottom, unsigned& left) const;

    WebPagePrivate* m_webPage;
    WebCore::IntPoint m_contentPos;
    TargetType m_targetType;
    MatchingApproachForClickable m_matchingApproach;
    CachedRectHitTestResults m_cachedRectHitTestResults;
};

}
}

#endif // FatFingers_h

