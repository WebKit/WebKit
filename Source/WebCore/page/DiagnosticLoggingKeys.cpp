/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
#include "DiagnosticLoggingKeys.h"

namespace WebCore {

const String& DiagnosticLoggingKeys::mediaLoadedKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("mediaLoaded"));
    return key;
}

const String& DiagnosticLoggingKeys::mediaLoadingFailedKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("mediaFailedLoading"));
    return key;
}

const String& DiagnosticLoggingKeys::pluginLoadedKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("pluginLoaded"));
    return key;
}

const String& DiagnosticLoggingKeys::pluginLoadingFailedKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("pluginFailedLoading"));
    return key;
}

const String& DiagnosticLoggingKeys::pageContainsPluginKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("pageContainsPlugin"));
    return key;
}

const String& DiagnosticLoggingKeys::pageContainsAtLeastOnePluginKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("pageContainsAtLeastOnePlugin"));
    return key;
}

const String& DiagnosticLoggingKeys::pageContainsMediaEngineKey()
{
    DEFINE_STATIC_LOCAL(const String, key, (String("pageContainsMediaEngine")));
    return key;
}

const String& DiagnosticLoggingKeys::pageContainsAtLeastOneMediaEngineKey()
{
    DEFINE_STATIC_LOCAL(const String, key, (String("pageContainsAtLeastOneMediaEngine")));
    return key;
}

const String& DiagnosticLoggingKeys::passKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("pass"));
    return key;
}

const String& DiagnosticLoggingKeys::failKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("fail"));
    return key;
}

const String& DiagnosticLoggingKeys::noopKey()
{
    DEFINE_STATIC_LOCAL(const String, key, ("noop"));
    return key;
}

}
