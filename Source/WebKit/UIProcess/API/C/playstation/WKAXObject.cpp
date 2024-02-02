/*
 * Copyright (C) 2023 Sony Interactive Entertainment Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "WKAXObject.h"

#include "WKAPICast.h"
#include "WKNumber.h"
#include "WebAccessibilityObject.h"
#include "WebFrameProxy.h"
#include "WebPageProxy.h"

using namespace WebKit;

WKTypeID WKAXObjectGetTypeID()
{
    return toAPI(WebAccessibilityObject::APIType);
}

WKAXRole WKAXObjectRole(WKAXObjectRef axObject)
{
    return toAPI(toImpl(axObject)->role());
}

WKStringRef WKAXObjectCopyTitle(WKAXObjectRef axObject)
{
    return toCopiedAPI(toImpl(axObject)->title());
}

WKStringRef WKAXObjectCopyDescription(WKAXObjectRef axObject)
{
    return toCopiedAPI(toImpl(axObject)->description());
}

WKStringRef WKAXObjectCopyHelpText(WKAXObjectRef axObject)
{
    return toCopiedAPI(toImpl(axObject)->helpText());
}

WKStringRef WKAXObjectCopyURL(WKAXObjectRef axObject)
{
    return toCopiedAPI(toImpl(axObject)->url().string());
}

WKAXButtonState WKAXObjectButtonState(WKAXObjectRef axObject)
{
    return toAPI(toImpl(axObject)->buttonState());
}

WKPageRef WKAXObjectPage(WKAXObjectRef axObject)
{
    return toAPI(toImpl(axObject)->page());
}

WKFrameRef WKAXObjectFrame(WKAXObjectRef axObject)
{
    return toAPI(toImpl(axObject)->frame());
}

WKRectRef WKAXObjectRect(WKAXObjectRef axObject)
{
    return WKRectCreate(toAPI(toImpl(axObject)->rect()));
}

WKStringRef WKAXObjectCopyValue(WKAXObjectRef axObject)
{
    return toCopiedAPI(toImpl(axObject)->value());
}

WKBooleanRef WKAXObjectIsFocused(WKAXObjectRef axObject)
{
    return WKBooleanCreate(toImpl(axObject)->isFocused());
}

WKBooleanRef WKAXObjectIsDisabled(WKAXObjectRef axObject)
{
    return WKBooleanCreate(toImpl(axObject)->isDisabled());
}

WKBooleanRef WKAXObjectIsSelected(WKAXObjectRef axObject)
{
    return WKBooleanCreate(toImpl(axObject)->isSelected());
}

WKBooleanRef WKAXObjectIsVisited(WKAXObjectRef axObject)
{
    return WKBooleanCreate(toImpl(axObject)->isVisited());
}

WKBooleanRef WKAXObjectIsLinked(WKAXObjectRef axObject)
{
    return WKBooleanCreate(toImpl(axObject)->isLinked());
}
