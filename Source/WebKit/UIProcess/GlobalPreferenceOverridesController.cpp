/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
#include "GlobalPreferenceOverridesController.h"

#include "Logging.h"
#include "WebPreferences.h"
#include <cmath>
#include <wtf/RunLoop.h>
#include <wtf/text/StringToIntegerConversion.h>

namespace WebKit {

GlobalPreferenceOverridesController& GlobalPreferenceOverridesController::singleton()
{
    static NeverDestroyed<GlobalPreferenceOverridesController> controller;
    return controller.get();
}

std::optional<WebPreferencesStore::Value> GlobalPreferenceOverridesController::parseOverride(const String& key, const String& value)
{
    auto& defaultValues = WebPreferencesStore::defaults();
    auto it = defaultValues.find(key);
    if (it == defaultValues.end())
        return std::nullopt;

    return WTF::switchOn(it->value,
        [&](bool) -> std::optional<WebPreferencesStore::Value> {
            if (value == "true"_s)
                return WebPreferencesStore::Value(true);
            if (value == "false"_s)
                return WebPreferencesStore::Value(false);
            return std::nullopt;
        },
        [&](uint32_t) -> std::optional<WebPreferencesStore::Value> {
            if (auto parsed = parseInteger<uint32_t>(value, 10, WTF::ParseIntegerWhitespacePolicy::Disallow))
                return WebPreferencesStore::Value(*parsed);
            return std::nullopt;
        },
        [&](double) -> std::optional<WebPreferencesStore::Value> {
            bool ok = false;
            double parsed = value.toDouble(&ok);
            if (ok && std::isfinite(parsed))
                return WebPreferencesStore::Value(parsed);
            return std::nullopt;
        },
        [&](const String&) -> std::optional<WebPreferencesStore::Value> {
            return WebPreferencesStore::Value(value);
        });
}

void GlobalPreferenceOverridesController::setOverrides(const HashMap<String, String>& delivered)
{
    assertIsMainRunLoop();

    WebPreferencesStore::ValueMap overrides;
    size_t unusableCount = 0;

    for (auto& [key, value] : delivered) {
        auto parsed = parseOverride(key, value);
        if (!parsed) {
            RELEASE_LOG_ERROR(Process, "GlobalPreferenceOverridesController: ignoring %" PUBLIC_LOG_STRING "=\"%" PUBLIC_LOG_STRING "\"; this build has no such preference, or the value is not of its type", key.utf8(), value.utf8());
            ++unusableCount;
            continue;
        }
        overrides.set(key, WTF::move(*parsed));
    }

    if (overrides == m_overrides) {
        RELEASE_LOG(Process, "GlobalPreferenceOverridesController: %u preference override(s) delivered, unchanged", overrides.size());
        return;
    }

    RELEASE_LOG(Process, "GlobalPreferenceOverridesController: applying %u preference override(s), %zu ignored", overrides.size(), unusableCount);

    m_overrides = WTF::move(overrides);

    WebPreferences::globalOverridesDidChange();
}

} // namespace WebKit
