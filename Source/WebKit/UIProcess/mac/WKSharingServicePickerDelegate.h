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

#if ENABLE(SERVICE_CONTROLS)

#import <wtf/RetainPtr.h>
#import <wtf/text/WTFString.h>

namespace WebKit {
class WebContextMenuProxyMac;
}

@class NSSharingServicePicker;

@interface WKSharingServicePickerDelegate : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate> {
    WebKit::WebContextMenuProxyMac* _menuProxy;
    RetainPtr<NSSharingServicePicker> _picker;
    BOOL _filterEditingServices;
    BOOL _handleEditingReplacement;
    NSRect _sourceFrame;
    String _attachmentID;
}

+ (WKSharingServicePickerDelegate *)sharedSharingServicePickerDelegate;
- (WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setMenuProxy:(WebKit::WebContextMenuProxyMac*)menuProxy;
- (void)setPicker:(NSSharingServicePicker *)picker;
- (void)setFiltersEditingServices:(BOOL)filtersEditingServices;
- (void)setHandlesEditingReplacement:(BOOL)handlesEditingReplacement;
- (void)setSourceFrame:(NSRect)sourceFrame;
- (void)setAttachmentID:(String)attachmentID;
- (void)removeBackground;

@end

#endif // ENABLE(SERVICE_CONTROLS)
