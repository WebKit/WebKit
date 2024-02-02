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

#pragma once

#include <WebKit/WKBase.h>

#ifdef __cplusplus
extern "C" {
#endif

WK_EXPORT WKTypeID WKAXObjectGetTypeID();
WK_EXPORT WKPageRef WKAXObjectPage(WKAXObjectRef);
WK_EXPORT WKFrameRef WKAXObjectFrame(WKAXObjectRef);
WK_EXPORT uint32_t WKAXObjectRole(WKAXObjectRef);
WK_EXPORT WKStringRef WKAXObjectCopyTitle(WKAXObjectRef);
WK_EXPORT WKStringRef WKAXObjectCopyDescription(WKAXObjectRef);
WK_EXPORT WKStringRef WKAXObjectCopyHelpText(WKAXObjectRef);
WK_EXPORT WKStringRef WKAXObjectCopyURL(WKAXObjectRef);
WK_EXPORT uint32_t WKAXObjectButtonState(WKAXObjectRef);
WK_EXPORT WKRectRef WKAXObjectRect(WKAXObjectRef);
WK_EXPORT WKStringRef WKAXObjectCopyValue(WKAXObjectRef);
WK_EXPORT WKBooleanRef WKAXObjectIsFocused(WKAXObjectRef);
WK_EXPORT WKBooleanRef WKAXObjectIsDisabled(WKAXObjectRef);
WK_EXPORT WKBooleanRef WKAXObjectIsSelected(WKAXObjectRef);
WK_EXPORT WKBooleanRef WKAXObjectIsVisited(WKAXObjectRef);
WK_EXPORT WKBooleanRef WKAXObjectIsLinked(WKAXObjectRef);

#ifdef __cplusplus
}
#endif
