/*
 * Copyright (C) 2023-2024 Apple Inc. All rights reserved.
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

#if ENABLE(WK_WEB_EXTENSIONS)

namespace WebKit {

// MARK: Alarms
static constexpr auto webExtensionMinimumAlarmInterval = 30_s;

// MARK: Message Passing
/// This matches the maximum message length enforced by Chromium in its `MessageFromJSONString()` function.
static constexpr size_t webExtensionMaxMessageLength = 1024 * 1024 * 64;

// MARK: Declarative Net Request
static constexpr size_t webExtensionDeclarativeNetRequestMaximumNumberOfStaticRulesets = 100;
static constexpr size_t webExtensionDeclarativeNetRequestMaximumNumberOfEnabledRulesets = 50;
static constexpr size_t webExtensionDeclarativeNetRequestMaximumNumberOfDynamicAndSessionRules = 30000;

// MARK: Storage
static constexpr double webExtensionUnlimitedStorageQuotaBytes = std::numeric_limits<double>::max();

static constexpr size_t webExtensionStorageAreaLocalQuotaBytes = 5 * 1024 * 1024;
static constexpr size_t webExtensionStorageAreaSessionQuotaBytes = 10 * 1024 * 1024;
static constexpr size_t webExtensionStorageAreaSyncQuotaBytes = 100 * 1024;

static constexpr size_t webExtensionStorageAreaSyncQuotaBytesPerItem = 8 * 1024;

static constexpr size_t webExtensionStorageAreaSyncMaximumItems = 512;
static constexpr size_t webExtensionStorageAreaSyncMaximumWriteOperationsPerHour = 1800;
static constexpr size_t webExtensionStorageAreaSyncMaximumWriteOperationsPerMinute = 120;

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
