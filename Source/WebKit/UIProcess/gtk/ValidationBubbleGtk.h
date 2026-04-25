/*
 * Copyright (C) 2025 Igalia S.L.
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

#include <WebCore/ValidationBubble.h>
#include <wtf/glib/GWeakPtr.h>

typedef struct _GtkWidget GtkWidget;

namespace WebKit {

class ValidationBubbleGtk final : public WebCore::ValidationBubble {
public:
    static Ref<ValidationBubbleGtk> create(GtkWidget* webView, String&& message, const WebCore::ValidationBubble::Settings& settings)
    {
        return adoptRef(*new ValidationBubbleGtk(webView, WTF::move(message), settings));
    }

    ~ValidationBubbleGtk();

private:
    ValidationBubbleGtk(GtkWidget*, String&&, const WebCore::ValidationBubble::Settings&);

    void showRelativeTo(const WebCore::IntRect&) final;

    void invalidate();

    GWeakPtr<GtkWidget> m_webView;
    GtkWidget* m_popover { nullptr };
};

} // namespace WebKit
