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

#import "config.h"
#import "AccessibilityCommonMac.h"

#import <JavaScriptCore/JSStringRefCF.h>

@implementation NSString (JSStringRefAdditions)

+ (NSString *)stringWithJSStringRef:(JSStringRef)jsStringRef
{
    if (!jsStringRef)
        return nil;

    return CFBridgingRelease(JSStringCopyCFString(kCFAllocatorDefault, jsStringRef));
}

- (JSRetainPtr<JSStringRef>)createJSStringRef
{
    return adopt(JSStringCreateWithCFString((__bridge CFStringRef)self));
}

NSDictionary *searchPredicateParameterizedAttributeForSearchCriteria(JSContextRef context, AccessibilityUIElement *startElement, bool isDirectionNext, unsigned resultsLimit, JSValueRef searchKey, JSStringRef searchText, bool visibleOnly, bool immediateDescendantsOnly)
{
    NSMutableDictionary *parameterizedAttribute = [NSMutableDictionary dictionary];
    
    if (startElement && startElement->platformUIElement())
        [parameterizedAttribute setObject:(id)startElement->platformUIElement() forKey:@"AXStartElement"];
    
    [parameterizedAttribute setObject:(isDirectionNext) ? @"AXDirectionNext" : @"AXDirectionPrevious" forKey:@"AXDirection"];
    
    [parameterizedAttribute setObject:@(resultsLimit) forKey:@"AXResultsLimit"];
    
    if (searchKey) {
        id searchKeyParameter = nil;
        if (JSValueIsString(context, searchKey)) {
            auto searchKeyString = adopt(JSValueToStringCopy(context, searchKey, nullptr));
            if (searchKeyString)
                searchKeyParameter = [NSString stringWithJSStringRef:searchKeyString.get()];
        } else if (JSValueIsObject(context, searchKey)) {
            JSObjectRef searchKeyArray = JSValueToObject(context, searchKey, nullptr);
            unsigned searchKeyArrayLength = 0;
            
            auto lengthPropertyString = adopt(JSStringCreateWithUTF8CString("length"));
            JSValueRef searchKeyArrayLengthValue = JSObjectGetProperty(context, searchKeyArray, lengthPropertyString.get(), nullptr);
            if (searchKeyArrayLengthValue && JSValueIsNumber(context, searchKeyArrayLengthValue))
                searchKeyArrayLength = static_cast<unsigned>(JSValueToNumber(context, searchKeyArrayLengthValue, nullptr));
            
            for (unsigned i = 0; i < searchKeyArrayLength; ++i) {
                JSValueRef searchKeyValue = JSObjectGetPropertyAtIndex(context, searchKeyArray, i, nullptr);
                JSStringRef searchKeyString = JSValueToStringCopy(context, searchKeyValue, nullptr);
                if (searchKeyString) {
                    if (!searchKeyParameter)
                        searchKeyParameter = [NSMutableArray array];
                    [searchKeyParameter addObject:[NSString stringWithJSStringRef:searchKeyString]];
                    JSStringRelease(searchKeyString);
                }
            }
        }
        if (searchKeyParameter)
            [parameterizedAttribute setObject:searchKeyParameter forKey:@"AXSearchKey"];
    }
    
    if (searchText && JSStringGetLength(searchText))
        [parameterizedAttribute setObject:[NSString stringWithJSStringRef:searchText] forKey:@"AXSearchText"];
    
    [parameterizedAttribute setObject:@(visibleOnly) forKey:@"AXVisibleOnly"];
    
    [parameterizedAttribute setObject:@(immediateDescendantsOnly) forKey:@"AXImmediateDescendantsOnly"];
    
    return parameterizedAttribute;
}


@end
