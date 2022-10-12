/*
 * Copyright (C) 2019 Igalia S.L.
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

#include "PointerLockManager.h"

#if PLATFORM(WAYLAND)

#include "relative-pointer-unstable-v1-client-protocol.h"
#include <WebCore/WlUniquePtr.h>
#include <wayland-client.h>

struct zwp_locked_pointer_v1;
struct zwp_pointer_constraints_v1;

namespace WebKit {

class WebPageProxy;

class PointerLockManagerWayland final : public PointerLockManager {
    WTF_MAKE_NONCOPYABLE(PointerLockManagerWayland); WTF_MAKE_FAST_ALLOCATED;
public:
    PointerLockManagerWayland(WebPageProxy&, const WebCore::FloatPoint&, const WebCore::FloatPoint&, WebMouseEventButton, unsigned short, OptionSet<WebEventModifier>);
    ~PointerLockManagerWayland();

private:
    static const struct wl_registry_listener s_registryListener;
    static const struct zwp_relative_pointer_v1_listener s_relativePointerListener;

    bool lock() override;
    bool unlock() override;

    WebCore::WlUniquePtr<struct wl_registry> m_registry;
    struct zwp_pointer_constraints_v1* m_pointerConstraints { nullptr };
    struct zwp_locked_pointer_v1* m_lockedPointer { nullptr };
    struct zwp_relative_pointer_manager_v1* m_relativePointerManager { nullptr };
    struct zwp_relative_pointer_v1* m_relativePointer { nullptr };
};

} // namespace WebKit

#endif // PLATFORM(WAYLAND)
