/*
 * Copyright (C) 2011 Nokia Corporation and/or its subsidiary(-ies)
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
#include "WebHitTestResult.h"

#include "WebCoreArgumentCoders.h"

#include <WebCore/KURL.h>
#include <wtf/text/WTFString.h>

using namespace WebCore;

namespace WebKit {

PassRefPtr<WebHitTestResult> WebHitTestResult::create(const WebHitTestResult::Data& hitTestResultData)
{
    return adoptRef(new WebHitTestResult(hitTestResultData));
}

void WebHitTestResult::Data::encode(CoreIPC::ArgumentEncoder& encoder) const
{
    encoder << absoluteImageURL;
    encoder << absolutePDFURL;
    encoder << absoluteLinkURL;
    encoder << absoluteMediaURL;
    encoder << linkLabel;
    encoder << linkTitle;
    encoder << isContentEditable;
    encoder << elementBoundingBox;
    encoder << isScrollbar;
}

bool WebHitTestResult::Data::decode(CoreIPC::ArgumentDecoder* decoder, WebHitTestResult::Data& hitTestResultData)
{
    if (!decoder->decode(hitTestResultData.absoluteImageURL)
        || !decoder->decode(hitTestResultData.absolutePDFURL)
        || !decoder->decode(hitTestResultData.absoluteLinkURL)
        || !decoder->decode(hitTestResultData.absoluteMediaURL)
        || !decoder->decode(hitTestResultData.linkLabel)
        || !decoder->decode(hitTestResultData.linkTitle)
        || !decoder->decode(hitTestResultData.isContentEditable)
        || !decoder->decode(hitTestResultData.elementBoundingBox)
        || !decoder->decode(hitTestResultData.isScrollbar))
        return false;

    return true;
}

} // WebKit
