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

#ifndef TouchEventHandler_h
#define TouchEventHandler_h

#include "ChromeClient.h"
#include "FatFingers.h"
#include "IntPoint.h"

#include <BlackBerryPlatformTouchEvent.h>

namespace BlackBerry {
namespace WebKit {

class WebPagePrivate;

class TouchEventHandler {
public:
    TouchEventHandler(WebPagePrivate* webpage);
    ~TouchEventHandler();

    bool handleTouchPoint(Platform::TouchPoint&, bool useFatFingers);
    void touchEventCancel();
    void touchHoldEvent();

    bool shouldSuppressMouseDownOnTouchDown() const;

    const FatFingersResult& lastFatFingersResult() const { return m_lastFatFingersResult; }
    void resetLastFatFingersResult() { m_lastFatFingersResult.reset(); }

private:
    void handleFatFingerPressed();

    void drawTapHighlight();

private:
    WebPagePrivate* m_webPage;

    bool m_didCancelTouch;
    bool m_convertTouchToMouse;

    WebCore::TouchEventMode m_existingTouchMode;

    WebCore::IntPoint m_lastScreenPoint; // Screen Position

    FatFingersResult m_lastFatFingersResult;
};

}
}

#endif // TouchEventHandler_h
