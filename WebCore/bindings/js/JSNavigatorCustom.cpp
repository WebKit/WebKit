/*
 *  Copyright (C) 2000 Harri Porten (porten@kde.org)
 *  Copyright (c) 2000 Daniel Molkentin (molkentin@kde.org)
 *  Copyright (c) 2000 Stefan Schimanski (schimmi@kde.org)
 *  Copyright (C) 2003, 2004, 2005, 2006, 2007, 2008 Apple Inc. All Rights Reserved.
 *  Copyright (C) 2008 Nokia Corporation and/or its subsidiary(-ies)
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

#include "config.h"
#include "JSNavigator.h"

#include "Frame.h"
#include "FrameLoader.h"
#include "KURL.h"
#include "Navigator.h"
#include "Settings.h"

namespace WebCore {

using namespace JSC;

static bool needsYouTubeQuirk(ExecState*, Frame*);

#if 1

static inline bool needsYouTubeQuirk(ExecState*, Frame*)
{
    return false;
}

#else

static bool needsYouTubeQuirk(ExecState* exec, Frame* frame)
{
    // This quirk works around a mistaken check in an ad at youtube.com.
    // There's a function called isSafari that returns false if the function
    // called isWindows returns true; thus the site malfunctions with Windows Safari.

    // Do the quirk only if the function's name is "isWindows".
    JSFunction* function = exec->function();
    if (!function)
        return false;
    static const Identifier& isWindowsFunctionName = *new Identifier(exec, "isWindows");
    if (function->functionName() != isWindowsFunctionName)
        return false;

    // Do the quirk only if the function is called by an "isSafari" function.
    // However, that function is not itself named -- it is stored in the isSafari
    // property, though, so that's how we recognize it.
    ExecState* callingExec = exec->callingExecState();
    if (!callingExec)
        return false;
    JSFunction* callingFunction = callingExec->function();
    if (!callingFunction)
        return false;
    JSObject* thisObject = callingExec->thisValue();
    if (!thisObject)
        return false;
    static const Identifier& isSafariFunctionName = *new Identifier(exec, "isSafari");
    JSValue* isSafariFunction = thisObject->getDirect(isSafariFunctionName);
    if (isSafariFunction != callingFunction)
        return false;

    Document* document = frame->document();
    // FIXME: The document is never null, so we should remove this check along with the
    // other similar ones in this file when we are absolutely sure it's safe.
    if (!document)
        return false;

    // Do the quirk only on the front page of the global version of YouTube.
    const KURL& url = document->url();
    if (url.host() != "youtube.com" && url.host() != "www.youtube.com")
        return false;
    if (url.path() != "/")
        return false;

    // As with other site-specific quirks, allow website developers to turn this off.
    // In theory, this allows website developers to check if their fixes are effective.
    Settings* settings = frame->settings();
    if (!settings)
        return false;
    if (!settings->needsSiteSpecificQuirks())
        return false;

    return true;
}

#endif

JSValue* JSNavigator::appVersion(ExecState* exec) const
{
    Navigator* imp = static_cast<Navigator*>(impl());
    Frame* frame = imp->frame();
    if (!frame)
        return jsString(exec, "");

    if (needsYouTubeQuirk(exec, frame))
        return jsString(exec, "");
    return jsString(exec, imp->appVersion());
}

void JSNavigator::mark()
{
    Base::mark();

    JSGlobalData& globalData = *Heap::heap(this)->globalData();

    markDOMObjectWrapper(globalData, impl()->optionalGeolocation());
}

}
