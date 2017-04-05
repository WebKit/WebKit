/*
 * Copyright (C) 2012 Igalia S.L.
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

#include "config.h"
#include "WebKitPrivate.h"

#include "APIError.h"
#include "WKErrorRef.h"
#include "WebEvent.h"
#include "WebKitError.h"
#include <gdk/gdk.h>

unsigned wkEventModifiersToGdkModifiers(WKEventModifiers wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers & kWKEventModifiersShiftKey)
        modifiers |= GDK_SHIFT_MASK;
    if (wkModifiers & kWKEventModifiersControlKey)
        modifiers |= GDK_CONTROL_MASK;
    if (wkModifiers & kWKEventModifiersAltKey)
        modifiers |= GDK_MOD1_MASK;
    if (wkModifiers & kWKEventModifiersMetaKey)
        modifiers |= GDK_META_MASK;
    if (wkModifiers & kWKEventModifiersCapsLockKey)
        modifiers |= GDK_LOCK_MASK;
    return modifiers;
}

unsigned toGdkModifiers(WebKit::WebEvent::Modifiers wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers & WebKit::WebEvent::Modifiers::ShiftKey)
        modifiers |= GDK_SHIFT_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::ControlKey)
        modifiers |= GDK_CONTROL_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::AltKey)
        modifiers |= GDK_MOD1_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::MetaKey)
        modifiers |= GDK_META_MASK;
    if (wkModifiers & WebKit::WebEvent::Modifiers::CapsLockKey)
        modifiers |= GDK_LOCK_MASK;
    return modifiers;
}

WebKitNavigationType toWebKitNavigationType(WebCore::NavigationType type)
{
    switch (type) {
    case WebCore::NavigationType::LinkClicked:
        return WEBKIT_NAVIGATION_TYPE_LINK_CLICKED;
    case WebCore::NavigationType::FormSubmitted:
        return WEBKIT_NAVIGATION_TYPE_FORM_SUBMITTED;
    case WebCore::NavigationType::BackForward:
        return WEBKIT_NAVIGATION_TYPE_BACK_FORWARD;
    case WebCore::NavigationType::Reload:
        return WEBKIT_NAVIGATION_TYPE_RELOAD;
    case WebCore::NavigationType::FormResubmitted:
        return WEBKIT_NAVIGATION_TYPE_FORM_RESUBMITTED;
    case WebCore::NavigationType::Other:
        return WEBKIT_NAVIGATION_TYPE_OTHER;
    default:
        ASSERT_NOT_REACHED();
        return WEBKIT_NAVIGATION_TYPE_OTHER;
    }
}

unsigned toWebKitMouseButton(WebKit::WebMouseEvent::Button button)
{
    switch (button) {
    case WebKit::WebMouseEvent::Button::NoButton:
        return 0;
    case WebKit::WebMouseEvent::Button::LeftButton:
        return 1;
    case WebKit::WebMouseEvent::Button::MiddleButton:
        return 2;
    case WebKit::WebMouseEvent::Button::RightButton:
        return 3;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned wkEventMouseButtonToWebKitMouseButton(WKEventMouseButton wkButton)
{
    switch (wkButton) {
    case kWKEventMouseButtonNoButton:
        return 0;
    case kWKEventMouseButtonLeftButton:
        return 1;
    case kWKEventMouseButtonMiddleButton:
        return 2;
    case kWKEventMouseButtonRightButton:
        return 3;
    }
    ASSERT_NOT_REACHED();
    return 0;
}

unsigned toWebKitError(unsigned webCoreError)
{
    switch (webCoreError) {
    case API::Error::Network::Cancelled:
        return WEBKIT_NETWORK_ERROR_CANCELLED;
    case API::Error::Network::FileDoesNotExist:
        return WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST;
    case kWKErrorCodeCannotShowMIMEType:
        return WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE;
    case kWKErrorCodeCannotShowURL:
        return WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI;
    case kWKErrorCodeFrameLoadInterruptedByPolicyChange:
        return WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE;
    case kWKErrorCodeCannotUseRestrictedPort:
        return WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT;
    case kWKErrorCodeCannotFindPlugIn:
        return WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN;
    case kWKErrorCodeCannotLoadPlugIn:
        return WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN;
    case kWKErrorCodeJavaUnavailable:
        return WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE;
    case kWKErrorCodePlugInCancelledConnection:
        return WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED;
    case kWKErrorCodePlugInWillHandleLoad:
        return WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD;
    case API::Error::Download::Transport:
        return WEBKIT_DOWNLOAD_ERROR_NETWORK;
    case API::Error::Download::CancelledByUser:
        return WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER;
    case API::Error::Download::Destination:
        return WEBKIT_DOWNLOAD_ERROR_DESTINATION;
    case API::Error::Print::General:
        return WEBKIT_PRINT_ERROR_GENERAL;
    case API::Error::Print::PrinterNotFound:
        return WEBKIT_PRINT_ERROR_PRINTER_NOT_FOUND;
    case API::Error::Print::InvalidPageRange:
        return WEBKIT_PRINT_ERROR_INVALID_PAGE_RANGE;
    default:
        // This may be a user app defined error, which needs to be passed as-is.
        return webCoreError;
    }
}

unsigned toWebCoreError(unsigned webKitError)
{
    switch (webKitError) {
    case WEBKIT_NETWORK_ERROR_CANCELLED:
        return API::Error::Network::Cancelled;
    case WEBKIT_NETWORK_ERROR_FILE_DOES_NOT_EXIST:
        return API::Error::Network::FileDoesNotExist;
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE:
        return kWKErrorCodeCannotShowMIMEType;
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI:
        return kWKErrorCodeCannotShowURL;
    case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
        return kWKErrorCodeFrameLoadInterruptedByPolicyChange;
    case WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT:
        return kWKErrorCodeCannotUseRestrictedPort;
    case WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN:
        return kWKErrorCodeCannotFindPlugIn;
    case WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN:
        return kWKErrorCodeCannotLoadPlugIn;
    case WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE:
        return kWKErrorCodeJavaUnavailable;
    case WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED:
        return kWKErrorCodePlugInCancelledConnection;
    case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
        return kWKErrorCodePlugInWillHandleLoad;
    case WEBKIT_DOWNLOAD_ERROR_NETWORK:
        return API::Error::Download::Transport;
    case WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER:
        return API::Error::Download::CancelledByUser;
    case WEBKIT_DOWNLOAD_ERROR_DESTINATION:
        return API::Error::Download::Destination;
    case WEBKIT_PRINT_ERROR_GENERAL:
        return API::Error::Print::General;
    case WEBKIT_PRINT_ERROR_PRINTER_NOT_FOUND:
        return API::Error::Print::PrinterNotFound;
    case WEBKIT_PRINT_ERROR_INVALID_PAGE_RANGE:
        return API::Error::Print::InvalidPageRange;
    default:
        // This may be a user app defined error, which needs to be passed as-is.
        return webKitError;
    }
}
