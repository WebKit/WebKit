/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT HOLDERS OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef WebColorPickerEfl_h
#define WebColorPickerEfl_h

#if ENABLE(INPUT_TYPE_COLOR)
#include "WebColorPicker.h"
#include "WebColorPickerResultListenerProxy.h"
#include "WebViewEfl.h"

namespace WebKit {

class WebColorPickerEfl : public WebColorPicker {
public:
    static PassRefPtr<WebColorPickerEfl> create(WebViewEfl* webView, WebColorPicker::Client* client, const WebCore::Color& initialColor)
    {
        return adoptRef(new WebColorPickerEfl(webView, client, initialColor));
    }
    ~WebColorPickerEfl() { }

    virtual void endPicker() override;
    virtual void setSelectedColor(const WebCore::Color&) override;
    virtual void showColorPicker(const WebCore::Color&) override;

    void didChooseColor(const WebCore::Color&);

private:
    WebColorPickerEfl(WebViewEfl*, WebColorPicker::Client*, const WebCore::Color&);

    WebViewEfl* m_webView;
    RefPtr<WebColorPickerResultListenerProxy> m_colorPickerResultListener;
};

}
#endif

#endif // WebColorPickerEfl_h
