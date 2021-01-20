/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
 * Copyright (C) 2019 Sony Interactive Entertainment Inc.
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
#include "EnvironmentUtilities.h"

#include <cstdlib>
#include <wtf/text/StringBuilder.h>

namespace WebKit {

namespace EnvironmentUtilities {

String stripEntriesEndingWith(StringView input, StringView suffix)
{
    StringBuilder output;

    auto hasAppended = false;
    for (auto entry : input.splitAllowingEmptyEntries(':')) {
        if (entry.endsWith(suffix))
            continue;

        if (hasAppended)
            output.append(':');
        else
            hasAppended = true;

        output.append(entry);
    }

    return output.toString();
}

void removeValuesEndingWith(const char* environmentVariable, const char* searchValue)
{
    const char* before = getenv(environmentVariable);
    if (!before)
        return;

    auto after = stripEntriesEndingWith(before, searchValue);
    if (after.isEmpty()) {
        unsetenv(environmentVariable);
        return;
    }

    setenv(environmentVariable, after.utf8().data(), 1);
}

} // namespace EnvironmentUtilities

} // namespace WebKit
