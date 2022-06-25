/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#import "AccessibilityUIElement.h"
#import <JavaScriptCore/JSRetainPtr.h>

// If an unsupported attribute is passed in, it will raise an accessibility exception. These are usually caught by the Accessibility Runtime to inform
// the AX client app of the error. However, DRT is the AX client app, so it must catch these exceptions.
#define BEGIN_AX_OBJC_EXCEPTIONS @try {
#define END_AX_OBJC_EXCEPTIONS } @catch(NSException *e) { \
    ASSERT_NOT_REACHED(); \
    if (![e.name isEqualToString:NSAccessibilityException]) \
        @throw; \
    }

@interface NSString (JSStringRefAdditions)
+ (NSString *)stringWithJSStringRef:(JSStringRef)jsStringRef;
- (JSRetainPtr<JSStringRef>)createJSStringRef;
@end

namespace WTR {

Class webAccessibilityObjectWrapperClass();
JSValueRef makeValueRefForValue(JSContextRef, id value);
extern NSDictionary *searchPredicateParameterizedAttributeForSearchCriteria(JSContextRef, AccessibilityUIElement *startElement, bool isDirectionNext, unsigned resultsLimit, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly);

};
