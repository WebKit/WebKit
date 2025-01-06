/*
 * Copyright (C) 2024 Igalia S.L.
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

#if USE(LIBDRM)

#include <wtf/ThreadSafeRefCounted.h>
#include <wtf/text/CString.h>
#include <wtf/unix/UnixFileDescriptor.h>

#if USE(GBM)
struct gbm_device;
#endif

namespace WTF {
class String;
}

namespace WebCore {

class DRMDeviceNode : public ThreadSafeRefCounted<DRMDeviceNode, WTF::DestructionThread::Main> {
public:
    static RefPtr<DRMDeviceNode> create(CString&&);
    ~DRMDeviceNode();

    const CString& filename() const { return m_filename; }

#if USE(GBM)
    struct gbm_device* gbmDevice() const;
#endif

private:
    explicit DRMDeviceNode(CString&&);

    CString m_filename;
#if USE(GBM)
    mutable WTF::UnixFileDescriptor m_fd;
    mutable std::optional<struct gbm_device*> m_gbmDevice;
#endif
};

} // namespace WebCore

#endif // USE(LIBDRM)
