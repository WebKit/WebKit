/*
 * Copyright (C) 2006 Apple Inc. All rights reserved.
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

#if PLATFORM(IOS)

#import "WebDefaultFormDelegate.h"

@implementation WebDefaultFormDelegate

+ (WebDefaultFormDelegate *)sharedFormDelegate
{
    static WebDefaultFormDelegate * WebSharedDefaultFormDelegate = nil;
    
    if (!WebSharedDefaultFormDelegate) WebSharedDefaultFormDelegate = [[WebDefaultFormDelegate alloc] init];
    return WebSharedDefaultFormDelegate;
}

- (void)textFieldDidBeginEditing:(DOMHTMLInputElement *)element inFrame:(WebFrame *)frame
{
}

- (void)textFieldDidEndEditing:(DOMHTMLInputElement *)element inFrame:(WebFrame *)frame
{
}

- (void)textDidChangeInTextField:(DOMHTMLInputElement *)element inFrame:(WebFrame *)frame
{
}

- (void)textDidChangeInTextArea:(DOMHTMLTextAreaElement *)element inFrame:(WebFrame *)frame
{
}

- (void)didFocusTextField:(DOMHTMLInputElement *)element inFrame:(WebFrame *)frame
{
}

- (BOOL)textField:(DOMHTMLInputElement *)element doCommandBySelector:(SEL)commandSelector inFrame:(WebFrame *)frame
{
    return YES;
}

#if !PLATFORM(IOS)
- (BOOL)textField:(DOMHTMLInputElement *)element shouldHandleEvent:(NSEvent *)event inFrame:(WebFrame *)frame
{
    return YES;
}
#endif

- (void)frame:(WebFrame *)frame sourceFrame:(WebFrame *)sourceFrame willSubmitForm:(DOMElement *)form withValues:(NSDictionary *)values submissionListener:(id <WebFormSubmissionListener>)listener
{
    [listener continue];
}

- (void)willSendSubmitEventToForm:(DOMHTMLFormElement *)element inFrame:(WebFrame *)sourceFrame withValues:(NSDictionary *)values
{
}

@end

#endif // PLATFORM(IOS)
