/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <wtf/text/WTFString.h>

namespace IPC {

class Validation {
public:
    static bool isValidFilePath(const String& path)
    {
        constexpr unsigned maxPathLength = 4096;
        if (path.length() >= maxPathLength)
            return false;

        for (unsigned i = 0; i < path.length(); ++i) {
            auto c = path[i];
            if (c < 0x20 || c == 0x7F)
                return false;
        }

        return true;
    }

    static bool isValidURLPath(const String& path)
    {
        if (path.isEmpty())
            return true;

        constexpr unsigned maxURLPathLength = 8192;
        if (path.length() >= maxURLPathLength)
            return false;

        for (unsigned i = 0; i < path.length(); ++i) {
            auto c = path[i];
            if (c < 0x20 || c == 0x7F)
                return false;
        }

        return true;
    }

    static bool isValidHostname(const String& hostname)
    {
        if (hostname.isEmpty())
            return true;

        constexpr unsigned maxHostnameLength = 253;
        if (hostname.length() > maxHostnameLength)
            return false;

        for (unsigned i = 0; i < hostname.length(); ++i) {
            auto c = hostname[i];
            if (c < 0x20 || c == 0x7F)
                return false;
        }

        if (hostname.find(".."_s) != notFound)
            return false;

        if (hostname.length() > 1) {
            if (hostname.startsWith('.') || hostname.endsWith('.'))
                return false;
        }

        auto labels = hostname.split('.');

        for (const auto& label : labels) {
            if (label.isEmpty())
                return false;

            constexpr unsigned maxLabelLength = 63;
            if (label.length() > maxLabelLength)
                return false;

            auto first = label[0];
            auto last = label[label.length() - 1];

            bool firstIsAlphaNum = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || (first >= '0' && first <= '9');
            bool lastIsAlphaNum = (last >= 'a' && last <= 'z') || (last >= 'A' && last <= 'Z') || (last >= '0' && last <= '9');

            if (!firstIsAlphaNum || !lastIsAlphaNum)
                return false;

            for (unsigned i = 0; i < label.length(); ++i) {
                auto c = label[i];
                bool isValid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '-';
                if (!isValid)
                    return false;
            }
        }

        return true;
    }

    static bool isValidMimeType(const String& mimeType)
    {
        if (mimeType.isEmpty())
            return true;

        constexpr unsigned maxMimeTypeLength = 256;
        if (mimeType.length() >= maxMimeTypeLength)
            return false;

        for (unsigned i = 0; i < mimeType.length(); ++i) {
            auto c = mimeType[i];
            if (c < 0x20 || c == 0x7F)
                return false;
        }

        auto slashPos = mimeType.find('/');
        if (slashPos == notFound)
            return false;

        if (slashPos == 0)
            return false;

        auto semicolonPos = mimeType.find(';');
        auto endOfSubtype = semicolonPos == notFound ? mimeType.length() : semicolonPos;

        if (slashPos + 1 >= endOfSubtype)
            return false;

        auto typeSubtype = mimeType.substring(0, endOfSubtype);
        if (typeSubtype[0] == ' ' || typeSubtype[typeSubtype.length() - 1] == ' ')
            return false;

        return true;
    }

    static bool isValidURLScheme(const String& scheme)
    {
        if (scheme.isEmpty())
            return false;

        constexpr unsigned maxSchemeLength = 32;
        if (scheme.length() >= maxSchemeLength)
            return false;

        auto first = scheme[0];
        bool firstIsLetter = (first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z');
        if (!firstIsLetter)
            return false;

        for (unsigned i = 0; i < scheme.length(); ++i) {
            auto c = scheme[i];
            bool isValid = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '+' || c == '-' || c == '.';
            if (!isValid)
                return false;
        }

        return true;
    }

    static bool isValidEmailAddress(const String& email)
    {
        if (email.isEmpty())
            return true;

        constexpr unsigned maxEmailLength = 320;
        constexpr unsigned maxLocalPartLength = 64;
        constexpr unsigned maxDomainLength = 255;

        if (email.find('\0') != notFound || email.length() > maxEmailLength)
            return false;

        auto atPosition = email.find('@');
        if (atPosition == notFound)
            return false;

        if (email.reverseFind('@') != atPosition)
            return false;

        if (atPosition == 0 || atPosition == email.length() - 1)
            return false;

        auto localPart = email.left(atPosition);
        auto domainPart = email.substring(atPosition + 1);

        if (localPart.length() > maxLocalPartLength || domainPart.length() > maxDomainLength)
            return false;

        if (email.find(".."_s) != notFound)
            return false;

        if (localPart.startsWith('.') || localPart.endsWith('.'))
            return false;

        if (domainPart.startsWith('.') || domainPart.endsWith('.'))
            return false;

        for (unsigned i = 0; i < email.length(); ++i) {
            auto c = email[i];
            if (c <= 0x20 || c == 0x7F)
                return false;
        }

        return true;
    }
};

} // namespace IPC
