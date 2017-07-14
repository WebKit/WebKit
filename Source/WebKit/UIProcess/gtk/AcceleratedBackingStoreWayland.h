/*
 * Copyright (C) 2016 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "AcceleratedBackingStore.h"

#if PLATFORM(WAYLAND)

#include <WebCore/RefPtrCairo.h>
#include <gtk/gtk.h>

namespace WebKit {

class WebPageProxy;

class AcceleratedBackingStoreWayland final : public AcceleratedBackingStore {
    WTF_MAKE_NONCOPYABLE(AcceleratedBackingStoreWayland); WTF_MAKE_FAST_ALLOCATED;
public:
    static std::unique_ptr<AcceleratedBackingStoreWayland> create(WebPageProxy&);
    ~AcceleratedBackingStoreWayland();

#if GTK_CHECK_VERSION(3, 16, 0)
    bool canGdkUseGL() const;
#endif

private:
    AcceleratedBackingStoreWayland(WebPageProxy&);

    bool paint(cairo_t*, const WebCore::IntRect&) override;

    RefPtr<cairo_surface_t> m_surface;
};

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
