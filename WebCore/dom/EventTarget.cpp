/*
 * This file is part of the DOM implementation for KDE.
 *
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 *           (C) 2001 Dirk Mueller (mueller@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov (ap@webkit.org)
 *           (C) 2007, 2008 Nikolas Zimmermann <zimmermann@kde.org>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"
#include "EventTarget.h"

namespace WebCore {

#ifndef NDEBUG
static int gEventDispatchForbidden = 0;
#endif

EventTarget::~EventTarget()
{
}

Node* EventTarget::toNode()
{
    return 0;
}

DOMWindow* EventTarget::toDOMWindow()
{
    return 0;
}

XMLHttpRequest* EventTarget::toXMLHttpRequest()
{
    return 0;
}

XMLHttpRequestUpload* EventTarget::toXMLHttpRequestUpload()
{
    return 0;
}

#if ENABLE(OFFLINE_WEB_APPLICATIONS)
DOMApplicationCache* EventTarget::toDOMApplicationCache()
{
    return 0;
}
#endif

#if ENABLE(SVG)
SVGElementInstance* EventTarget::toSVGElementInstance()
{
    return 0;
}
#endif

MessagePort* EventTarget::toMessagePort()
{
    return 0;
}

#if ENABLE(WORKERS)
Worker* EventTarget::toWorker()
{
    return 0;
}

WorkerContext* EventTarget::toWorkerContext()
{
    return 0;
}
#endif

#ifndef NDEBUG
void forbidEventDispatch()
{
    ++gEventDispatchForbidden;
}

void allowEventDispatch()
{
    if (gEventDispatchForbidden > 0)
        --gEventDispatchForbidden;
}

bool eventDispatchForbidden()
{
    return gEventDispatchForbidden > 0;
}
#endif // NDEBUG

} // end namespace
