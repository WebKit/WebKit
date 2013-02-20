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

#ifndef SelectionHandler_h
#define SelectionHandler_h

#include "BlackBerryPlatformIntRectRegion.h"
#include "BlackBerryPlatformPrimitives.h"
#include "BlackBerryPlatformStopWatch.h"
#include "TextGranularity.h"

#include <wtf/Vector.h>

namespace WTF {
class String;
}

namespace WebCore {
class FloatQuad;
class IntPoint;
class IntRect;
class Node;
class VisiblePosition;
class VisibleSelection;
}

namespace BlackBerry {
namespace Platform {
class String;
}

namespace WebKit {

class FatFingersResult;
class WebPagePrivate;

class SelectionHandler {
public:
    SelectionHandler(WebPagePrivate*);
    ~SelectionHandler();

    bool isSelectionActive() { return m_selectionActive; }
    void setSelectionActive(bool active) { m_selectionActive = active; }

    void cancelSelection();
    BlackBerry::Platform::String selectedText() const;

    bool selectionContains(const WebCore::IntPoint&);

    void setSelection(const WebCore::IntPoint& start, const WebCore::IntPoint& end);
    void selectAtPoint(const WebCore::IntPoint&);
    void selectObject(const WebCore::IntPoint&, WebCore::TextGranularity);
    void selectObject(WebCore::TextGranularity);
    void selectObject(WebCore::Node*);

    void selectionPositionChanged(bool forceUpdateWithoutChange = false);

    void setCaretPosition(const WebCore::IntPoint&);

    bool lastUpdatedEndPointIsValid() const { return m_lastUpdatedEndPointIsValid; }

    void inputHandlerDidFinishProcessingChange();

private:
    void notifyCaretPositionChangedIfNeeded(bool userTouchTriggered = true);
    void caretPositionChanged(bool userTouchTriggered);
    void regionForTextQuads(WTF::Vector<WebCore::FloatQuad>&, BlackBerry::Platform::IntRectRegion&, bool shouldClipToVisibleContent = true) const;
    WebCore::IntRect clippingRectForVisibleContent() const;
    bool updateOrHandleInputSelection(WebCore::VisibleSelection& newSelection, const WebCore::IntPoint& relativeStart
                                      , const WebCore::IntPoint& relativeEnd);
    WebCore::Node* DOMContainerNodeForVisiblePosition(const WebCore::VisiblePosition&) const;
    bool shouldUpdateSelectionOrCaretForPoint(const WebCore::IntPoint&, const WebCore::IntRect&, bool startCaret = true) const;
    unsigned extendSelectionToFieldBoundary(bool isStartHandle, const WebCore::IntPoint& selectionPoint, WebCore::VisibleSelection& newSelection);
    WebCore::IntPoint clipPointToVisibleContainer(const WebCore::IntPoint&) const;

    bool inputNodeOverridesTouch() const;
    BlackBerry::Platform::RequestedHandlePosition requestedSelectionHandlePosition(const WebCore::VisibleSelection&) const;

    bool selectNodeIfFatFingersResultIsLink(FatFingersResult);

    WebPagePrivate* m_webPage;

    bool m_selectionActive;
    bool m_caretActive;
    bool m_lastUpdatedEndPointIsValid;
    bool m_didSuppressCaretPositionChangedNotification;
    BlackBerry::Platform::IntRectRegion m_lastSelectionRegion;

    BlackBerry::Platform::StopWatch m_timer;
};

}
}

#endif // SelectionHandler_h
