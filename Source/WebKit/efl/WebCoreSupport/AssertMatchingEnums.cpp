/*
 * Copyright (C) 2011 Samsung Electronics
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

// Use this file to assert that various WebKit API enum values continue
// matching WebCore defined enum values.

#include "config.h"

#include "Page.h"
#include "VisibleSelection.h"
#include "ewk_frame.h"
#include "ewk_view.h"
#include <wtf/Assertions.h>

#if ENABLE(TOUCH_EVENTS)
#include "PlatformTouchEvent.h"
#include "PlatformTouchPoint.h"
#endif

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(static_cast<int>(webkit_name) == static_cast<int>(WebCore::webcore_name), mismatching_enums)

#if ENABLE(PAGE_VISIBILITY_API)
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGE_VISIBILITY_STATE_VISIBLE, PageVisibilityStateVisible);
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGE_VISIBILITY_STATE_HIDDEN, PageVisibilityStateHidden);
COMPILE_ASSERT_MATCHING_ENUM(EWK_PAGE_VISIBILITY_STATE_PRERENDER, PageVisibilityStatePrerender);
#endif

COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_SELECTION_NONE, VisibleSelection::NoSelection);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_SELECTION_CARET, VisibleSelection::CaretSelection);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TEXT_SELECTION_RANGE, VisibleSelection::RangeSelection);

#if ENABLE(TOUCH_EVENTS)
COMPILE_ASSERT_MATCHING_ENUM(EWK_TOUCH_POINT_RELEASED, PlatformTouchPoint::TouchReleased);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TOUCH_POINT_PRESSED, PlatformTouchPoint::TouchPressed);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TOUCH_POINT_MOVED, PlatformTouchPoint::TouchMoved);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TOUCH_POINT_STATIONARY, PlatformTouchPoint::TouchStationary);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TOUCH_POINT_CANCELLED, PlatformTouchPoint::TouchCancelled);
COMPILE_ASSERT_MATCHING_ENUM(EWK_TOUCH_POINT_END, PlatformTouchPoint::TouchStateEnd);
#endif

COMPILE_ASSERT_MATCHING_ENUM(EWK_VIEW_MODE_INVALID, Page::ViewModeInvalid);
COMPILE_ASSERT_MATCHING_ENUM(EWK_VIEW_MODE_WINDOWED, Page::ViewModeWindowed);
COMPILE_ASSERT_MATCHING_ENUM(EWK_VIEW_MODE_FLOATING, Page::ViewModeFloating);
COMPILE_ASSERT_MATCHING_ENUM(EWK_VIEW_MODE_FULLSCREEN, Page::ViewModeFullscreen);
COMPILE_ASSERT_MATCHING_ENUM(EWK_VIEW_MODE_MAXIMIZED, Page::ViewModeMaximized);
COMPILE_ASSERT_MATCHING_ENUM(EWK_VIEW_MODE_MINIMIZED, Page::ViewModeMinimized);
