/*
 * Copyright (C) 2012, 2013 Adobe Systems Incorporated. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY,
 * OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
 * TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
 * THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#pragma once

#include "CanvasPath.h"
#include "SVGPathUtilities.h"
#include <wtf/RefCounted.h>

namespace WebCore {

struct DOMMatrix2DInit;

class WEBCORE_EXPORT Path2D final : public RefCounted<Path2D>, public CanvasPath {
    WTF_MAKE_FAST_ALLOCATED;
public:
    virtual ~Path2D();

    static Ref<Path2D> create()
    {
        return adoptRef(*new Path2D);
    }

    static Ref<Path2D> create(const Path& path)
    {
        return adoptRef(*new Path2D(path));
    }

    static Ref<Path2D> create(const Path2D& path)
    {
        return create(path.path());
    }

    static Ref<Path2D> create(const String& pathData)
    {
        return create(buildPathFromString(pathData));
    }

    ExceptionOr<void> addPath(Path2D&, DOMMatrix2DInit&&);

    const Path& path() const { return m_path; }

private:
    Path2D() = default;
    Path2D(const Path& path)
        : CanvasPath(path)
    {
    }
};

} // namespace WebCore
