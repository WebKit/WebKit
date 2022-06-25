/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
#include "WKGeometry.h"

#include "APIGeometry.h"
#include "WKAPICast.h"

WKTypeID WKSizeGetTypeID(void)
{
    return WebKit::toAPI(API::Size::APIType);
}

WKTypeID WKPointGetTypeID(void)
{
    return WebKit::toAPI(API::Point::APIType);
}

WKTypeID WKRectGetTypeID(void)
{
    return WebKit::toAPI(API::Rect::APIType);
}

WKPointRef WKPointCreate(WKPoint point)
{
    return WebKit::toAPI(&API::Point::create(point).leakRef());
}

WKSizeRef WKSizeCreate(WKSize size)
{
    return WebKit::toAPI(&API::Size::create(size).leakRef());
}

WKRectRef WKRectCreate(WKRect rect)
{
    return WebKit::toAPI(&API::Rect::create(rect).leakRef());
}

WKSize WKSizeGetValue(WKSizeRef size)
{
    return WebKit::toImpl(size)->size();
}

WKPoint WKPointGetValue(WKPointRef point)
{
    return WebKit::toImpl(point)->point();
}

WKRect WKRectGetValue(WKRectRef rect)
{
    return WebKit::toImpl(rect)->rect();
}

