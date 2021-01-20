/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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

#define errorNameAndDetailsSeparator ";"

// Make sure the predefined error name is valid, otherwise use InternalError.
#define VALIDATED_ERROR_MESSAGE(errorString) Inspector::Protocol::AutomationHelpers::parseEnumValueFromString<Inspector::Protocol::Automation::ErrorMessage>(errorString).valueOr(Inspector::Protocol::Automation::ErrorMessage::InternalError)

// If the error name is incorrect for these macros, it will be a compile-time error.
#define STRING_FOR_PREDEFINED_ERROR_NAME(errorName) Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::errorName)
#define STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(errorName, detailsString) makeString(Inspector::Protocol::AutomationHelpers::getEnumConstantValue(Inspector::Protocol::Automation::ErrorMessage::errorName), errorNameAndDetailsSeparator, detailsString)

// If the error message is not a predefined error, InternalError will be used instead.
#define STRING_FOR_PREDEFINED_ERROR_MESSAGE(errorMessage) Inspector::Protocol::AutomationHelpers::getEnumConstantValue(VALIDATED_ERROR_MESSAGE(errorMessage))
#define STRING_FOR_PREDEFINED_ERROR_MESSAGE_AND_DETAILS(errorMessage, detailsString) makeString(Inspector::Protocol::AutomationHelpers::getEnumConstantValue(VALIDATED_ERROR_MESSAGE(errorMessage)), errorNameAndDetailsSeparator, detailsString)

#define AUTOMATION_COMMAND_ERROR_WITH_NAME(errorName) AutomationCommandError(Inspector::Protocol::Automation::ErrorMessage::errorName)
#define AUTOMATION_COMMAND_ERROR_WITH_MESSAGE(errorString) AutomationCommandError(VALIDATED_ERROR_MESSAGE(errorString))

// Convenience macros for filling in the error string of synchronous commands in bailout branches.
#define SYNC_FAIL_WITH_PREDEFINED_ERROR(errorName) \
do { \
    return makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME(errorName)); \
} while (false)

#define SYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(errorName, detailsString) \
do { \
    return makeUnexpected(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(errorName, detailsString)); \
} while (false)

#define ASYNC_FAIL_WITH_PREDEFINED_ERROR(errorName) \
do { \
    callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME(errorName)); \
    return; \
} while (false)

#define ASYNC_FAIL_WITH_PREDEFINED_ERROR_AND_DETAILS(errorName, detailsString) \
do { \
    callback->sendFailure(STRING_FOR_PREDEFINED_ERROR_NAME_AND_DETAILS(errorName, detailsString)); \
    return; \
} while (false)
