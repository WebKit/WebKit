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

#pragma once

#include "WebKitScriptDialog.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

struct _WebKitScriptDialog {
    _WebKitScriptDialog(unsigned type, const CString& message, const CString& defaultText, Function<void(bool, const String&)>&& completionHandler)
        : type(type)
        , message(message)
        , defaultText(defaultText)
        , completionHandler(WTFMove(completionHandler))
    {
    }

    unsigned type;
    CString message;
    CString defaultText;

    bool confirmed { false };
    CString text;

    Function<void(bool, const String&)> completionHandler;

#if PLATFORM(GTK)
    GtkWidget* nativeDialog { nullptr };
#endif

    int referenceCount { 1 };
};

WebKitScriptDialog* webkitScriptDialogCreate(unsigned type, const CString& message, const CString& defaultText, Function<void(bool, const String&)>&& completionHandler);
bool webkitScriptDialogIsRunning(WebKitScriptDialog*);
void webkitScriptDialogAccept(WebKitScriptDialog*);
void webkitScriptDialogDismiss(WebKitScriptDialog*);
void webkitScriptDialogSetUserInput(WebKitScriptDialog*, const String&);
