/*
 * Copyright (C) 2018 Metrological Group B.V.
 * Copyright (C) 2018 Igalia S.L.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "NicosiaImageBackingStore.h"
#include <wtf/Lock.h>
#include <wtf/ThreadSafeRefCounted.h>

namespace Nicosia {

class ImageBacking : public ThreadSafeRefCounted<ImageBacking> {
public:
    static Ref<ImageBacking> create()
    {
        return adoptRef(*new ImageBacking());
    }

    // A move-only update container.
    struct Update {
        Update() = default;
        Update(const Update&) = delete;
        Update& operator=(const Update&) = delete;
        Update(Update&&) = default;
        Update& operator=(Update&&) = default;

        bool isVisible { false };
        uintptr_t nativeImageID { 0 };
        RefPtr<Nicosia::ImageBackingStore> imageBackingStore;
    };

    // An immutable layer-side state object. flushUpdate() prepares
    // the current update for consumption by the composition-side.
    struct LayerState {
        LayerState() = default;
        LayerState(const LayerState&) = delete;
        LayerState& operator=(const LayerState&) = delete;
        LayerState(LayerState&&) = delete;
        LayerState& operator=(LayerState&&) = delete;

        uintptr_t imageID { 0 };
        Update update;
    };
    LayerState& layerState() { return m_layerState; }

    void flushUpdate();

    // An immutable composition-side state object. takeUpdate() returns the update
    // information that's to be fed to the CoordinatedBackingStore object.
    struct CompositionState {
        CompositionState() = default;
        CompositionState(const CompositionState&) = delete;
        CompositionState& operator=(const CompositionState&) = delete;
        CompositionState(CompositionState&&) = delete;
        CompositionState& operator=(CompositionState&&) = delete;

        RefPtr<ImageBackingStore> imageBackingStore;
    };
    CompositionState& compositionState() { return m_compositionState; }

    Update takeUpdate();

private:
    LayerState m_layerState;
    CompositionState m_compositionState;

    struct {
        Lock lock;
        Update update WTF_GUARDED_BY_LOCK(lock);
    } m_update;
};

} // namespace Nicosia
