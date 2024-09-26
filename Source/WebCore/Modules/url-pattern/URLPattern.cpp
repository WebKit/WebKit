/*
 * Copyright (C) 2023 Apple Inc. All rights reserved.
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
#include <wtf/RefCounted.h>
#include "URLPatternInit.h"
#include "URLPatternOptions.h"
#include "URLPattern.h"

namespace WebCore {

WTF_MAKE_TZONE_OR_ISO_ALLOCATED_IMPL(URLPattern);

 Ref<URLPattern> URLPattern::create(const URLPatternInput input, const String&& baseURL, URLPatternOptions&& options)
 {
 	(void) input;  // Need to add this so build won't complain about unused variables
 	(void) baseURL;
    (void) options;

     return adoptRef(*new URLPattern());
 }

 Ref<URLPattern> URLPattern::create(std::optional<URLPatternInput> input, URLPatternOptions&& options)
{
    (void) options;

     if (input.has_value())
     {
     	;
     }

    return adoptRef(*new URLPattern());
}

URLPattern::URLPattern() = default;

URLPattern::~URLPattern() = default;

ExceptionOr<bool> URLPattern::test(std::optional<URLPatternInput> input, std::optional<String>&& baseURL)
{
    
	if (input.has_value())
	{
		;
	}


	if (baseURL.has_value())
	{
		;
	}

    return Exception { ExceptionCode::NotSupportedError };
    // return false;
}

// TODO
// URLPatternResult URLPattern::exec(std::optional<String>&& input, std::optional<String>&& baseURL)
// {
//     return;
// }


}
