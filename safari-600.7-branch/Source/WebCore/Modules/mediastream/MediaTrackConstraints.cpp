/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 *
 */

#include "config.h"

#if ENABLE(MEDIA_STREAM)

#include "MediaTrackConstraints.h"

#include "MediaTrackConstraint.h"
#include "MediaTrackConstraintSet.h"
#include "NotImplemented.h"

using namespace JSC;

namespace WebCore {

RefPtr<MediaTrackConstraints> MediaTrackConstraints::create(PassRefPtr<MediaConstraintsImpl> constraints)
{
    return adoptRef(new MediaTrackConstraints(constraints));
}

MediaTrackConstraints::MediaTrackConstraints(PassRefPtr<MediaConstraintsImpl> constraints)
    : m_constraints(constraints)
{
}

Vector<PassRefPtr<MediaTrackConstraint>> MediaTrackConstraints::optional(bool) const
{
    // https://bugs.webkit.org/show_bug.cgi?id=121954
    notImplemented();
    return Vector<PassRefPtr<MediaTrackConstraint>>();
}

RefPtr<MediaTrackConstraintSet> MediaTrackConstraints::mandatory(bool) const
{
    // https://bugs.webkit.org/show_bug.cgi?id=121954
    notImplemented();
    return nullptr;
}

} // namespace WebCore

#endif
