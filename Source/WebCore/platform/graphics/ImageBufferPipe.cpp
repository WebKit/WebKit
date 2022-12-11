/*
 * Copyright (C) 2020 Igalia S.L
 * Copyright (C) 2020 Metrological Group B.V.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "ImageBufferPipe.h"

#include "GraphicsLayerContentsDisplayDelegate.h"
#include <wtf/Lock.h>

namespace WebCore {

#if !USE(NICOSIA)

class ImageBufferPipeSourceDelegate : public ImageBufferPipe::Source {
public:
    ImageBufferPipeSourceDelegate()
    { }

    void handle(ImageBuffer& image) final
    {
        Locker locker { m_lock };
        if (m_delegate)
            m_delegate->tryCopyToLayer(image);
    }

    Lock m_lock;
    RefPtr<GraphicsLayerAsyncContentsDisplayDelegate> m_delegate WTF_GUARDED_BY_LOCK(m_lock);
};

class ImageBufferPipeDelegate : public ImageBufferPipe {
public:
    ImageBufferPipeDelegate()
        : m_source(adoptRef(*new ImageBufferPipeSourceDelegate))
    {
    }

    void setContentsToLayer(GraphicsLayer& layer) final
    {
        Locker locker { m_source->m_lock };
        if (!m_source->m_delegate)
            m_source->m_delegate = layer.createAsyncContentsDisplayDelegate();
    }

    RefPtr<ImageBufferPipe::Source> source() const final
    {
        return m_source.ptr();
    }

    Ref<ImageBufferPipeSourceDelegate> m_source;
};

RefPtr<ImageBufferPipe> ImageBufferPipe::create()
{
    return adoptRef(new ImageBufferPipeDelegate);
}

#endif

} // namespace WebCore
