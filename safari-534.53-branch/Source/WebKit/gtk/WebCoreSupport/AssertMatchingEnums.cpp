/*
 *  Copyright (C) 2011 Collabora Ltd.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

// Use this file to assert that various WebKit API enum values continue
// matching WebCore defined enum values.

#include "config.h"

#include "DumpRenderTreeSupportGtk.h"
#include "EditingBehaviorTypes.h"
#include "FindOptions.h"
#include "FrameLoaderTypes.h"
#include "webkitwebnavigationaction.h"
#include "webkitwebsettings.h"
#include <wtf/Assertions.h>

#define COMPILE_ASSERT_MATCHING_ENUM(webkit_name, webcore_name) \
    COMPILE_ASSERT(int(webkit_name) == int(WebCore::webcore_name), mismatching_enums)

COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_EDITING_BEHAVIOR_MAC, EditingMacBehavior);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_EDITING_BEHAVIOR_WINDOWS, EditingWindowsBehavior);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_EDITING_BEHAVIOR_UNIX, EditingUnixBehavior);

COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_WEB_NAVIGATION_REASON_LINK_CLICKED, NavigationTypeLinkClicked);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_WEB_NAVIGATION_REASON_FORM_SUBMITTED, NavigationTypeFormSubmitted);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_WEB_NAVIGATION_REASON_BACK_FORWARD, NavigationTypeBackForward);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_WEB_NAVIGATION_REASON_RELOAD, NavigationTypeReload);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_WEB_NAVIGATION_REASON_FORM_RESUBMITTED, NavigationTypeFormResubmitted);
COMPILE_ASSERT_MATCHING_ENUM(WEBKIT_WEB_NAVIGATION_REASON_OTHER, NavigationTypeOther);

COMPILE_ASSERT_MATCHING_ENUM(WebKit::WebFindOptionsAtWordStarts, AtWordStarts);
COMPILE_ASSERT_MATCHING_ENUM(WebKit::WebFindOptionsTreatMedialCapitalAsWordStart, TreatMedialCapitalAsWordStart);
COMPILE_ASSERT_MATCHING_ENUM(WebKit::WebFindOptionsBackwards, Backwards);
COMPILE_ASSERT_MATCHING_ENUM(WebKit::WebFindOptionsWrapAround, WrapAround);
COMPILE_ASSERT_MATCHING_ENUM(WebKit::WebFindOptionsStartInSelection, StartInSelection);
