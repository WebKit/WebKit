/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#if ENABLE(DATALIST_ELEMENT) && USE(APPKIT)

#import "WebDataListSuggestionsDropdown.h"
#import <wtf/RetainPtr.h>

OBJC_CLASS WKDataListSuggestionsView;

namespace WebKit {

class WebDataListSuggestionsDropdownMac final : public WebDataListSuggestionsDropdown {
public:
    static Ref<WebDataListSuggestionsDropdownMac> create(WebPageProxy&, NSView *);
    ~WebDataListSuggestionsDropdownMac();

    void didSelectOption(const String&);

private:
    WebDataListSuggestionsDropdownMac(WebPageProxy&, NSView *);

    void show(WebCore::DataListSuggestionInformation&&) final;
    void handleKeydownWithIdentifier(const String&) final;
    void close() final;

    void selectOption();

    NSView *m_view;
    RetainPtr<WKDataListSuggestionsView> m_dropdownUI;
};

} // namespace WebKit

#endif // ENABLE(DATALIST_ELEMENT) && USE(APPKIT)
