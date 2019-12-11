/*
 * Copyright (C) 2015 Igalia S.L.
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

#ifndef WebKitConsoleMessagePrivate_h
#define WebKitConsoleMessagePrivate_h

#include "WebKitConsoleMessage.h"
#include <JavaScriptCore/ConsoleTypes.h>
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

struct _WebKitConsoleMessage {
    _WebKitConsoleMessage(JSC::MessageSource source, JSC::MessageLevel level, const String& message, unsigned lineNumber, const String& sourceID)
        : source(source)
        , level(level)
        , message(message.utf8())
        , lineNumber(lineNumber)
        , sourceID(sourceID.utf8())
    {
    }

    _WebKitConsoleMessage(WebKitConsoleMessage* consoleMessage)
        : source(consoleMessage->source)
        , level(consoleMessage->level)
        , message(consoleMessage->message)
        , lineNumber(consoleMessage->lineNumber)
        , sourceID(consoleMessage->sourceID)
    {
    }

    JSC::MessageSource source;
    JSC::MessageLevel level;
    CString message;
    unsigned lineNumber;
    CString sourceID;
};

#endif // WebKitConsoleMessagePrivate_h
