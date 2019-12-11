/*
 * Copyright (C) 2015 Apple Inc. All rights reserved.
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
#include "ContentExtensionError.h"

#if ENABLE(CONTENT_EXTENSIONS)

#include <string>
#include <wtf/NeverDestroyed.h>

namespace WebCore {
namespace ContentExtensions {

const char* WebKitContentBlockerDomain = "WebKitContentBlockerDomain";

const std::error_category& contentExtensionErrorCategory()
{
    class ContentExtensionErrorCategory : public std::error_category {
        const char* name() const noexcept override
        {
            return "content extension";
        }

        std::string message(int errorCode) const override
        {
            switch (static_cast<ContentExtensionError>(errorCode)) {
            case ContentExtensionError::JSONInvalid:
                return "Failed to parse the JSON String.";
            case ContentExtensionError::JSONTopLevelStructureNotAnObject:
                return "Invalid input, the top level structure is not an object.";
            case ContentExtensionError::JSONTopLevelStructureNotAnArray:
                return "Invalid input, the top level structure is not an array.";
            case ContentExtensionError::JSONInvalidObjectInTopLevelArray:
                return "Invalid object in the top level array.";
            case ContentExtensionError::JSONInvalidRule:
                return "Invalid rule.";
            case ContentExtensionError::JSONContainsNoRules:
                return "Empty extension.";
            case ContentExtensionError::JSONInvalidTrigger:
                return "Invalid trigger object.";
            case ContentExtensionError::JSONInvalidURLFilterInTrigger:
                return "Invalid url-filter object.";
            case ContentExtensionError::JSONInvalidTriggerFlagsArray:
                return "Invalid trigger flags array.";
            case ContentExtensionError::JSONInvalidObjectInTriggerFlagsArray:
                return "Invalid object in the trigger flags array.";
            case ContentExtensionError::JSONInvalidStringInTriggerFlagsArray:
                return "Invalid string in the trigger flags array.";
            case ContentExtensionError::JSONInvalidAction:
                return "Invalid action object.";
            case ContentExtensionError::JSONInvalidActionType:
                return "Invalid action type.";
            case ContentExtensionError::JSONInvalidCSSDisplayNoneActionType:
                return "Invalid css-display-none action type. Requires a selector.";
            case ContentExtensionError::JSONInvalidRegex:
                return "Invalid or unsupported regular expression.";
            case ContentExtensionError::JSONInvalidConditionList:
                return "Invalid list of if-domain, unless-domain, if-top-url, or unless-top-url conditions.";
            case ContentExtensionError::JSONTooManyRules:
                return "Too many rules in JSON array.";
            case ContentExtensionError::JSONDomainNotLowerCaseASCII:
                return "Domains must be lower case ASCII. Use punycode to encode non-ASCII characters.";
            case ContentExtensionError::JSONMultipleConditions:
                return "A trigger cannot have more than one condition (if-domain, unless-domain, if-top-url, or unless-top-url)";
            case ContentExtensionError::JSONTopURLAndDomainConditions:
                return "A list cannot have if-domain and unless-domain mixed with if-top-url and unless-top-url";
            case ContentExtensionError::JSONInvalidNotification:
                return "A notify action must have a string notification";
            }

            return std::string();
        }
    };

    static NeverDestroyed<ContentExtensionErrorCategory> contentExtensionErrorCategory;
    return contentExtensionErrorCategory;
}

} // namespace ContentExtensions
} // namespace WebCore

#endif // ENABLE(CONTENT_EXTENSIONS)
