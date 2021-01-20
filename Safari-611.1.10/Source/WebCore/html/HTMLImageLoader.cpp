/*
 * Copyright (C) 1999 Lars Knoll (knoll@kde.org)
 *           (C) 1999 Antti Koivisto (koivisto@kde.org)
 * Copyright (C) 2004, 2005, 2006, 2007, 2010, 2015 Apple Inc. All rights reserved.
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
#include "HTMLImageLoader.h"

#include "CachedImage.h"
#include "CommonVM.h"
#include "DOMWindow.h"
#include "Element.h"
#include "Event.h"
#include "EventNames.h"
#include "HTMLNames.h"
#include "HTMLObjectElement.h"
#include "HTMLParserIdioms.h"
#include "HTMLVideoElement.h"
#include "Settings.h"

#include "JSDOMWindowBase.h"
#include <JavaScriptCore/JSCInlines.h>
#include <JavaScriptCore/JSLock.h>

namespace WebCore {

HTMLImageLoader::HTMLImageLoader(Element& element)
    : ImageLoader(element)
{
}

HTMLImageLoader::~HTMLImageLoader() = default;

void HTMLImageLoader::dispatchLoadEvent()
{
#if ENABLE(VIDEO)
    // HTMLVideoElement uses this class to load the poster image, but it should not fire events for loading or failure.
    if (is<HTMLVideoElement>(element()))
        return;
#endif

    bool errorOccurred = image()->errorOccurred();
    if (!errorOccurred && image()->response().httpStatusCode() >= 400)
        errorOccurred = is<HTMLObjectElement>(element()); // An <object> considers a 404 to be an error and should fire onerror.
    element().dispatchEvent(Event::create(errorOccurred ? eventNames().errorEvent : eventNames().loadEvent, Event::CanBubble::No, Event::IsCancelable::No));
}

String HTMLImageLoader::sourceURI(const AtomString& attr) const
{
    return stripLeadingAndTrailingHTMLSpaces(attr);
}

void HTMLImageLoader::notifyFinished(CachedResource&, const NetworkLoadMetrics& metrics)
{
    ASSERT(image());
    CachedImage& cachedImage = *image();

    Ref<Element> protect(element());
    ImageLoader::notifyFinished(cachedImage, metrics);

    bool loadError = cachedImage.errorOccurred() || cachedImage.response().httpStatusCode() >= 400;
    if (!loadError) {
        if (!element().isConnected()) {
            JSC::VM& vm = commonVM();
            JSC::JSLockHolder lock(vm);
            // FIXME: Adopt reportExtraMemoryVisited, and switch to reportExtraMemoryAllocated.
            // https://bugs.webkit.org/show_bug.cgi?id=142595
            vm.heap.deprecatedReportExtraMemory(cachedImage.encodedSize());
        }
    }

    if (loadError && is<HTMLObjectElement>(element()))
        downcast<HTMLObjectElement>(element()).renderFallbackContent();
}

}
