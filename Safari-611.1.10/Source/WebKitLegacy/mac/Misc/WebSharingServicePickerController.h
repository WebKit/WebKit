/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#if PLATFORM(MAC)
#import <pal/spi/mac/NSSharingServicePickerSPI.h>
#import <pal/spi/mac/NSSharingServiceSPI.h>
#endif

@class WebSharingServicePickerController;
@class WebView;

namespace WebCore {
class FloatRect;
class Page;
}

class WebContextMenuClient;

class WebSharingServicePickerClient {
public:
    virtual ~WebSharingServicePickerClient() { }

    virtual void sharingServicePickerWillBeDestroyed(WebSharingServicePickerController &);
    virtual WebCore::Page* pageForSharingServicePicker(WebSharingServicePickerController &);
    virtual RetainPtr<NSWindow> windowForSharingServicePicker(WebSharingServicePickerController &);

    virtual WebCore::FloatRect screenRectForCurrentSharingServicePickerItem(WebSharingServicePickerController &);
    virtual RetainPtr<NSImage> imageForCurrentSharingServicePickerItem(WebSharingServicePickerController &);

    WebView *webView() { return m_webView; }

protected:
    explicit WebSharingServicePickerClient(WebView *);
    WebView *m_webView;
};

@interface WebSharingServicePickerController : NSObject <NSSharingServiceDelegate, NSSharingServicePickerDelegate> {
    WebSharingServicePickerClient* _pickerClient;
    RetainPtr<NSSharingServicePicker> _picker;
    BOOL _includeEditorServices;
    BOOL _handleEditingReplacement;
}

- (instancetype)initWithItems:(NSArray *)items includeEditorServices:(BOOL)includeEditorServices client:(WebSharingServicePickerClient*)pickerClient style:(NSSharingServicePickerStyle)style;
- (instancetype)initWithSharingServicePicker:(NSSharingServicePicker *)sharingServicePicker client:(WebSharingServicePickerClient&)pickerClient;
- (NSMenu *)menu;
- (void)didShareImageData:(NSData *)data confirmDataIsValidTIFFData:(BOOL)confirmData;
- (void)clear;

@end

#endif
