/*
 * Copyright (c) 2013 The Chromium Authors. All rights reserved.
 * Copyright (C) 2013 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *    * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *    * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *    * Neither the name of Google Inc. nor the names of its
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

#ifndef WebColorPickerMac_h
#define WebColorPickerMac_h

#if ENABLE(INPUT_TYPE_COLOR)

#if USE(APPKIT)

#import "WebColorPicker.h"
#import "WKView.h"
#import <WebCore/IntRect.h>
#include <wtf/RetainPtr.h>

namespace WebCore {
class Color;
}

namespace WebKit {
class WebColorPickerMac;
}

@protocol WKColorPickerUIMac <NSObject>
- (void)setAndShowPicker:(WebKit::WebColorPickerMac*)picker withColor:(NSColor *)color;
- (void)invalidate;
- (void)setColor:(NSColor *)color;
- (void)didChooseColor:(id)sender;
@end

namespace WebKit {
    
class WebColorPickerMac : public WebColorPicker {
public:        
    static PassRefPtr<WebColorPickerMac> create(WebColorPicker::Client*, const WebCore::Color&, const WebCore::IntRect&, WKView*);
    ~WebColorPickerMac();

    virtual void endPicker() override;
    virtual void setSelectedColor(const WebCore::Color&) override;
    virtual void showColorPicker(const WebCore::Color&) override;
    
    void didChooseColor(const WebCore::Color&);

private:
    WebColorPickerMac(WebColorPicker::Client*, const WebCore::Color&, const WebCore::IntRect&, WKView*);
    RetainPtr<NSObject<WKColorPickerUIMac> > m_colorPickerUI;
};

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)

#endif // USE(APPKIT)

#endif // WebContextMenuProxyMac_h
