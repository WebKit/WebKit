/*
 * Copyright (C) 2012-2013 Nokia Corporation and/or its subsidiary(-ies).
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "PlatformScreen.h"

#include "FloatRect.h"
#include "NotImplemented.h"
#include "Widget.h"
#include <wtf/text/CString.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

int screenHorizontalDPI(Widget*)
{
    notImplemented();
    return 0;
}

int screenVerticalDPI(Widget*)
{
    notImplemented();
    return 0;
}

int screenDepth(Widget*)
{
    notImplemented();
    return 0;
}

int screenDepthPerComponent(Widget*)
{
    notImplemented();
    return 0;
}

bool screenIsMonochrome(Widget*)
{
    notImplemented();
    return false;
}

FloatRect screenRect(Widget*)
{
    notImplemented();
    return FloatRect(0, 0, 1024, 768);
}

FloatRect screenAvailableRect(Widget* widget)
{
    notImplemented();
    return screenRect(widget);
}

void screenColorProfile(ColorProfile&)
{
    notImplemented();
}

}
