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
#include "WebEvent.h"
#include "WebKitError.h"

#if PLATFORM(GTK)
#include <WebCore/GtkVersioning.h>
#elif PLATFORM(WPE)
#include <wpe/wpe.h>
#endif

#if PLATFORM(GTK)
unsigned toPlatformModifiers(OptionSet<WebKit::WebEventModifier> wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers.contains(WebKit::WebEventModifier::ShiftKey))
        modifiers |= GDK_SHIFT_MASK;
    if (wkModifiers.contains(WebKit::WebEventModifier::ControlKey))
        modifiers |= GDK_CONTROL_MASK;
    if (wkModifiers.contains(WebKit::WebEventModifier::AltKey))
        modifiers |= GDK_MOD1_MASK;
    if (wkModifiers.contains(WebKit::WebEventModifier::MetaKey))
        modifiers |= GDK_META_MASK;
    if (wkModifiers.contains(WebKit::WebEventModifier::CapsLockKey))
        modifiers |= GDK_LOCK_MASK;
    return modifiers;
}
#elif PLATFORM(WPE)
unsigned toPlatformModifiers(OptionSet<WebKit::WebEventModifier> wkModifiers)
{
    unsigned modifiers = 0;
    if (wkModifiers.contains(WebKit::WebEventModifier::ShiftKey))
        modifiers |= wpe_input_keyboard_modifier_shift;
    if (wkModifiers.contains(WebKit::WebEventModifier::ControlKey))
        modifiers |= wpe_input_keyboard_modifier_control;
    if (wkModifiers.contains(WebKit::WebEventModifier::AltKey))
        modifiers |= wpe_input_keyboard_modifier_alt;
    if (wkModifiers.contains(WebKit::WebEventModifier::MetaKey))
        modifiers |= wpe_input_keyboard_modifier_meta;
    return modifiers;
}
#endif

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

unsigned toWebKitMouseButton(WebKit::WebMouseEventButton button)
{
    switch (button) {
    case WebKit::WebMouseEventButton::NoButton:
        return 0;
    case WebKit::WebMouseEventButton::LeftButton:
        return 1;
    case WebKit::WebMouseEventButton::MiddleButton:
        return 2;
    case WebKit::WebMouseEventButton::RightButton:
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
    case API::Error::Policy::CannotShowMIMEType:
        return WEBKIT_POLICY_ERROR_CANNOT_SHOW_MIME_TYPE;
    case API::Error::Policy::CannotShowURL:
        return WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI;
    case API::Error::Policy::FrameLoadInterruptedByPolicyChange:
        return WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE;
    case API::Error::Policy::CannotUseRestrictedPort:
        return WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT;
#if !ENABLE(2022_GLIB_API)
    case API::Error::Plugin::CannotFindPlugIn:
        return WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN;
    case API::Error::Plugin::CannotLoadPlugIn:
        return WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN;
    case API::Error::Plugin::JavaUnavailable:
        return WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE;
    case API::Error::Plugin::PlugInCancelledConnection:
        return WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED;
    case API::Error::Plugin::PlugInWillHandleLoad:
        return WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD;
#endif
    case API::Error::Download::Transport:
        return WEBKIT_DOWNLOAD_ERROR_NETWORK;
    case API::Error::Download::CancelledByUser:
        return WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER;
    case API::Error::Download::Destination:
        return WEBKIT_DOWNLOAD_ERROR_DESTINATION;
#if PLATFORM(GTK)
    case API::Error::Print::Generic:
        return WEBKIT_PRINT_ERROR_GENERAL;
    case API::Error::Print::PrinterNotFound:
        return WEBKIT_PRINT_ERROR_PRINTER_NOT_FOUND;
    case API::Error::Print::InvalidPageRange:
        return WEBKIT_PRINT_ERROR_INVALID_PAGE_RANGE;
#endif
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
        return API::Error::Policy::CannotShowMIMEType;
    case WEBKIT_POLICY_ERROR_CANNOT_SHOW_URI:
        return API::Error::Policy::CannotShowURL;
    case WEBKIT_POLICY_ERROR_FRAME_LOAD_INTERRUPTED_BY_POLICY_CHANGE:
        return API::Error::Policy::FrameLoadInterruptedByPolicyChange;
    case WEBKIT_POLICY_ERROR_CANNOT_USE_RESTRICTED_PORT:
        return API::Error::Policy::CannotUseRestrictedPort;
#if !ENABLE(2022_GLIB_API)
    case WEBKIT_PLUGIN_ERROR_CANNOT_FIND_PLUGIN:
        return API::Error::Plugin::CannotFindPlugIn;
    case WEBKIT_PLUGIN_ERROR_CANNOT_LOAD_PLUGIN:
        return API::Error::Plugin::CannotLoadPlugIn;
    case WEBKIT_PLUGIN_ERROR_JAVA_UNAVAILABLE:
        return API::Error::Plugin::JavaUnavailable;
    case WEBKIT_PLUGIN_ERROR_CONNECTION_CANCELLED:
        return API::Error::Plugin::PlugInCancelledConnection;
    case WEBKIT_PLUGIN_ERROR_WILL_HANDLE_LOAD:
        return API::Error::Plugin::PlugInWillHandleLoad;
#endif
    case WEBKIT_DOWNLOAD_ERROR_NETWORK:
        return API::Error::Download::Transport;
    case WEBKIT_DOWNLOAD_ERROR_CANCELLED_BY_USER:
        return API::Error::Download::CancelledByUser;
    case WEBKIT_DOWNLOAD_ERROR_DESTINATION:
        return API::Error::Download::Destination;
#if PLATFORM(GTK)
    case WEBKIT_PRINT_ERROR_GENERAL:
        return API::Error::Print::Generic;
    case WEBKIT_PRINT_ERROR_PRINTER_NOT_FOUND:
        return API::Error::Print::PrinterNotFound;
    case WEBKIT_PRINT_ERROR_INVALID_PAGE_RANGE:
        return API::Error::Print::InvalidPageRange;
#endif
    default:
        // This may be a user app defined error, which needs to be passed as-is.
        return webKitError;
    }
}
