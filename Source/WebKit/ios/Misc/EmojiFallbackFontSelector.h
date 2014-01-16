/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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

#ifndef EmojiFallbackFontSelector_h
#define EmojiFallbackFontSelector_h

#include <WebCore/FontSelector.h>
#include <wtf/PassRefPtr.h>

class EmojiFallbackFontSelector : public WebCore::FontSelector {
public:
    static PassRefPtr<EmojiFallbackFontSelector> create() { return adoptRef(new EmojiFallbackFontSelector()); }
    virtual ~EmojiFallbackFontSelector() override { }
    virtual PassRefPtr<WebCore::FontData> getFontData(const WebCore::FontDescription&, const AtomicString& familyName) override { ASSERT_NOT_REACHED(); return 0; }
    virtual bool resolvesFamilyFor(const WebCore::FontDescription&) const override { ASSERT_NOT_REACHED(); return false; }
    virtual size_t fallbackFontDataCount() override { return 1; };
    virtual PassRefPtr<WebCore::FontData> getFallbackFontData(const WebCore::FontDescription&, size_t) override;

    virtual void registerForInvalidationCallbacks(WebCore::FontSelectorClient*) override { }
    virtual void unregisterForInvalidationCallbacks(WebCore::FontSelectorClient*) override { }

    virtual unsigned version() const override { return 0; }
    virtual unsigned uniqueId() const override { return 0; }

private:
    EmojiFallbackFontSelector() { }
};

#endif // EmojiFallbackFontSelector_h
