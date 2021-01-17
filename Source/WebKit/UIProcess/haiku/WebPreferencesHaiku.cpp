/*
 * Copyright (C) 2011 Samsung Electronics
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
#include "WebPreferences.h"

#include <WebCore/NotImplemented.h>

namespace WebKit {

void WebPreferences::platformInitializeStore()
{
    notImplemented();
}

void WebPreferences::platformUpdateStringValueForKey(const String&, const String&)
{
    notImplemented();
}

void WebPreferences::platformUpdateBoolValueForKey(const String&, bool)
{
    notImplemented();
}

void WebPreferences::platformUpdateUInt32ValueForKey(const String&, uint32_t)
{
    notImplemented();
}

void WebPreferences::platformUpdateDoubleValueForKey(const String&, double)
{
    notImplemented();
}

void WebPreferences::platformUpdateFloatValueForKey(const String&, float)
{
    notImplemented();
}

void WebPreferences::platformDeleteKey(const String&)
{
    notImplemented();
}

bool WebPreferences::platformGetStringUserValueForKey(const String&, String&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetBoolUserValueForKey(const String&, bool&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetUInt32UserValueForKey(const String&, uint32_t&)
{
    notImplemented();
    return false;
}

bool WebPreferences::platformGetDoubleUserValueForKey(const String&, double&)
{
    notImplemented();
    return false;
}

} // namespace WebKit

