/*
 * Copyright (C) 2012 Intel Corporation. All rights reserved.
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

#if ENABLE(INPUT_TYPE_COLOR)

#include <WebCore/ColorChooser.h>
#include <wtf/CheckedPtr.h>
#include <wtf/RefCounted.h>
#include <wtf/TZoneMallocInlines.h>
#include <wtf/WeakPtr.h>

namespace WebCore {
class Color;
class ColorChooserClient;
}

namespace WebKit {

class WebPage;

class WebColorChooser : public RefCounted<WebColorChooser>, public WebCore::ColorChooser {
    WTF_MAKE_TZONE_ALLOCATED_INLINE(WebColorChooser);
public:
    static Ref<WebColorChooser> create(WebPage* page, WebCore::ColorChooserClient* client, const WebCore::Color& initialColor)
    {
        return adoptRef(*new WebColorChooser(page, client, initialColor));
    }

    virtual ~WebColorChooser();

    void ref() const final { RefCounted::ref(); }
    void deref() const final { RefCounted::deref(); }

    void didChooseColor(const WebCore::Color&);
    void didEndChooser();
    void disconnectFromPage();

    void reattachColorChooser(const WebCore::Color&) override;
    void setSelectedColor(const WebCore::Color&) override;
    void endChooser() override;

private:
    WebColorChooser(WebPage*, WebCore::ColorChooserClient*, const WebCore::Color&);

    CheckedPtr<WebCore::ColorChooserClient> m_colorChooserClient;
    WeakPtr<WebPage> m_page;
};

} // namespace WebKit

#endif // ENABLE(INPUT_TYPE_COLOR)
