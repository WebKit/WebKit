/*
 * Copyright (C) 2015-2026 Apple Inc. All rights reserved.
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

#include "config.h"
#include "Path2D.h"

#include "AffineTransform.h"
#include "DOMMatrix2DInit.h"
#include "DOMMatrixReadOnly.h"
#include "SVGPathUtilities.h"
#include <wtf/TZoneMallocInlines.h>

namespace WebCore {

WTF_MAKE_TZONE_ALLOCATED_IMPL(Path2D);

Path2D::~Path2D() = default;

Ref<Path2D> Path2D::create(std::optional<PathInit>&& path)
{
    if (!path)
        return adoptRef(*new Path2D);

    return WTF::switchOn(WTF::move(*path),
        [](Ref<Path2D>&& other) {
            return create(other->path());
        },
        [](String&& pathData) {
            return create(buildPathFromString(pathData));
        });
}

ExceptionOr<void> Path2D::addPath(Path2D& path, DOMMatrix2DInit&& matrixInit)
{
    auto transform = DOMMatrixReadOnly::toAffineTransform(matrixInit);
    if (transform.hasException())
        return transform.releaseException();

    m_path.addPath(path.path(), transform.releaseReturnValue());
    return { };
}

}
