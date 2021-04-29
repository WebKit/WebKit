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

#if ENABLE(DATALIST_ELEMENT) && PLATFORM(IOS_FAMILY)

#import "UIKitSPI.h"
#import "WebDataListSuggestionsDropdown.h"
#import <wtf/RetainPtr.h>
#import <wtf/Vector.h>

OBJC_CLASS WKContentView;

@interface WKDataListTextSuggestion : UITextSuggestion
@end

@interface WKDataListSuggestionsControl : NSObject

@property (nonatomic, readonly) BOOL isShowingSuggestions;

- (instancetype)initWithInformation:(WebCore::DataListSuggestionInformation&&)information inView:(WKContentView *)view;
- (void)updateWithInformation:(WebCore::DataListSuggestionInformation&&)information;
- (void)didSelectOptionAtIndex:(NSInteger)index;
- (void)invalidate;

@end

namespace WebKit {

class WebDataListSuggestionsDropdownIOS : public WebDataListSuggestionsDropdown {
public:
    static Ref<WebDataListSuggestionsDropdownIOS> create(WebPageProxy&, WKContentView *);

    void didSelectOption(const String&);

private:
    WebDataListSuggestionsDropdownIOS(WebPageProxy&, WKContentView *);

    void show(WebCore::DataListSuggestionInformation&&) final;
    void handleKeydownWithIdentifier(const String&) final;
    void close() final;

    WKContentView *m_contentView;
    RetainPtr<WKDataListSuggestionsControl> m_suggestionsControl;
};

} // namespace WebKit

#endif // ENABLE(DATALIST_ELEMENT) && PLATFORM(IOS_FAMILY)
