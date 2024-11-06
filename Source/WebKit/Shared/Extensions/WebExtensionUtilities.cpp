/*
 * Copyright (C) 2024 Igalia S.L. All rights reserved.
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
#include "WebExtensionUtilities.h"

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

Ref<JSON::Array> filterObjects(const JSON::Array& array, WTF::Function<bool(const JSON::Value&)>&& lambda)
{
    auto result = JSON::Array::create();

    for (Ref value : array) {
        if (!value)
            continue;

        if (lambda(value))
            result->pushValue(WTFMove(value));
    }

    return result;
}

Vector<String> makeStringVector(const JSON::Array& array)
{
    Vector<String> vector;
    size_t count = array.length();
    vector.reserveInitialCapacity(count);

    for (Ref value : array) {
        if (auto string = value->asString(); !string.isNull())
            vector.append(WTFMove(string));
    }

    vector.shrinkToFit();
    return vector;
}

double largestDisplayScale()
{
    auto largestDisplayScale = 1.0;

    for (double scale : availableScreenScales()) {
        if (scale > largestDisplayScale)
            largestDisplayScale = scale;
    }

    return largestDisplayScale;
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
