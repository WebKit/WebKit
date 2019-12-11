/*
 * Copyright (C) 2016 Apple Inc. All rights reserved.
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

#include "RenderTheme.h"
#include <wtf/RetainPtr.h>

#if ENABLE(MEDIA_CONTROLS_SCRIPT)
OBJC_CLASS NSDateComponentsFormatter;
#endif

namespace WebCore {

class RenderThemeCocoa : public RenderTheme {
private:
    bool shouldHaveCapsLockIndicator(const HTMLInputElement&) const final;

#if ENABLE(APPLE_PAY)
    void adjustApplePayButtonStyle(RenderStyle&, const Element*) const override;
    bool paintApplePayButton(const RenderObject&, const PaintInfo&, const IntRect&) override;
#endif

protected:
#if ENABLE(VIDEO)
    String mediaControlsFormattedStringForDuration(double) override;
#endif
#if ENABLE(MEDIA_CONTROLS_SCRIPT)
    RetainPtr<NSDateComponentsFormatter> m_durationFormatter;
#endif
};

}
