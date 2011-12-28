/*
 * Copyright (c) 2009, Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are
 * met:
 *
 *     * Redistributions of source code must retain the above copyright
 * notice, this list of conditions and the following disclaimer.
 *     * Redistributions in binary form must reproduce the above
 * copyright notice, this list of conditions and the following disclaimer
 * in the documentation and/or other materials provided with the
 * distribution.
 *     * Neither the name of Google Inc. nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef HarfBuzzSkia_h
#define HarfBuzzSkia_h

#include <wtf/RefCounted.h>
#include <wtf/RefPtr.h>

struct HB_FaceRec_;
struct HB_Font_;

namespace WebCore {

class FontPlatformData;

class HarfbuzzFace : public RefCounted<HarfbuzzFace> {
public:
    static PassRefPtr<HarfbuzzFace> create(FontPlatformData* platformData)
    {
        return adoptRef(new HarfbuzzFace(platformData));
    }

    ~HarfbuzzFace();

    HB_FaceRec_* face() const { return m_harfbuzzFace; }

private:
    explicit HarfbuzzFace(FontPlatformData*);

    uint32_t m_uniqueID;
    HB_FaceRec_* m_harfbuzzFace;
};

// FIXME: Remove this asymmetric alloc/free function.
// We'll remove this once we move to the new Harfbuzz API.
HB_Font_* allocHarfbuzzFont();

} // namespace WebCore

#endif
