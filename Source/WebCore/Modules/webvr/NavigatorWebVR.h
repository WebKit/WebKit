/*
 * Copyright (C) 2017-2018 Igalia S.L. All rights reserved.
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

#include "JSDOMPromiseDeferred.h"
#include "Supplementable.h"
#include <wtf/Vector.h>

namespace WebCore {

class Document;
class Navigator;
class Page;
class VRDisplay;

class NavigatorWebVR final : public Supplement<Navigator> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    using GetVRDisplaysPromise = DOMPromiseDeferred<IDLSequence<IDLInterface<VRDisplay>>>;

    static void getVRDisplays(Navigator&, Document&, GetVRDisplaysPromise&&);
    static const Vector<RefPtr<VRDisplay>>& activeVRDisplays(Navigator&);
    static bool vrEnabled(Navigator&);

    void getVRDisplays(Document&, GetVRDisplaysPromise&&);

private:
    static NavigatorWebVR* from(Navigator*);
    static const char* supplementName();

    Vector<Ref<VRDisplay>> m_displays;
};

} // namespace WebCore
