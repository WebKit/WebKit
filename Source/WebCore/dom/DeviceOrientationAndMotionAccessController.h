/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *  * Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *  * Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#if ENABLE(DEVICE_ORIENTATION)

#include "DeviceOrientationOrMotionPermissionState.h"
#include "ExceptionOr.h"
#include "SecurityOriginData.h"
#include <wtf/Function.h>
#include <wtf/Vector.h>
#include <wtf/WeakPtr.h>

namespace WebCore {

class Document;
class Page;

class DeviceOrientationAndMotionAccessController : public CanMakeWeakPtr<DeviceOrientationAndMotionAccessController> {
    WTF_MAKE_FAST_ALLOCATED;
public:
    explicit DeviceOrientationAndMotionAccessController(Document& topDocument);

    DeviceOrientationOrMotionPermissionState accessState(const Document&) const;
    void shouldAllowAccess(const Document&, Function<void(DeviceOrientationOrMotionPermissionState)>&&);

private:
    WeakRef<Document, WeakPtrImplWithEventTargetData> m_topDocument;
    HashMap<SecurityOriginData, DeviceOrientationOrMotionPermissionState> m_accessStatePerOrigin;
};

} // namespace WebCore

#endif // ENABLE(DEVICE_ORIENTATION)
