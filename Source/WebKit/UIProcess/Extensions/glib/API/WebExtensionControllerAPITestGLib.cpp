/*
 * Copyright (C) 2026 Igalia, S.L. All rights reserved.
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
#include "WebExtensionController.h"

#if ENABLE(WK_WEB_EXTENSIONS)

#include "Logging.h"

namespace WebKit {

void WebExtensionController::testResult(bool result, String message, String sourceURL, unsigned lineNumber)
{
    if (message.isEmpty())
        message = "(no message)"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test assertion passed: %s (%s:%u)", message.utf8(), sourceURL.utf8(), lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test assertion failed: %s (%s:%u)", message.utf8(), sourceURL.utf8(), lineNumber);
}

void WebExtensionController::testEqual(bool result, String expectedValue, String actualValue, String message, String sourceURL, unsigned lineNumber)
{
    if (message.isEmpty())
        message = "Expected equality of these values"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test equality passed: %s: %s === %s (%s:%u)", message.utf8(), expectedValue.utf8(), actualValue.utf8(), sourceURL.utf8(), lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test equality failed: %s: %s !== %s (%s:%u)", message.utf8(), expectedValue.utf8(), actualValue.utf8(), sourceURL.utf8(), lineNumber);
}

void WebExtensionController::testLogMessage(String message, String sourceURL, unsigned lineNumber)
{
    if (message.isEmpty())
        message = "(no message)"_s;

    RELEASE_LOG_INFO(Extensions, "Test log: %s (%s:%u)", message.utf8(), sourceURL.utf8(), lineNumber);
}

void WebExtensionController::testSentMessage(String message, String argument, String sourceURL, unsigned lineNumber)
{
    RELEASE_LOG_INFO(Extensions, "Test sent message: %s %s (%s:%u)", message.utf8(), argument.utf8(), sourceURL.utf8(), lineNumber);
}

void WebExtensionController::testAdded(String testName, String sourceURL, unsigned lineNumber)
{
    RELEASE_LOG_INFO(Extensions, "Test added: %s (%s:%u)", testName.utf8(), sourceURL.utf8(), lineNumber);
}

void WebExtensionController::testStarted(String testName, String sourceURL, unsigned lineNumber)
{
    RELEASE_LOG_INFO(Extensions, "Test started: %s (%s:%u)", testName.utf8(), sourceURL.utf8(), lineNumber);
}

void WebExtensionController::testFinished(String testName, bool result, String message, String sourceURL, unsigned lineNumber)
{
    if (testName.isEmpty())
        testName = "(no test name)"_s;

    if (message.isEmpty())
        message = "(no message)"_s;

    if (result) {
        RELEASE_LOG_INFO(Extensions, "Test passed: %s %s (%s:%u)", testName.utf8(), message.utf8(), sourceURL.utf8(), lineNumber);
        return;
    }

    RELEASE_LOG_ERROR(Extensions, "Test failed: %s %s (%s:%u)", testName.utf8(), message.utf8(), sourceURL.utf8(), lineNumber);
}

} // namespace WebKit

#endif // ENABLE(WK_WEB_EXTENSIONS)
