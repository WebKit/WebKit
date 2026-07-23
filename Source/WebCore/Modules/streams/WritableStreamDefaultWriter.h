/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
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
 */

#pragma once

#include "InternalWritableStreamWriter.h"
#include <wtf/RefCounted.h>

namespace WebCore {

class InternalWritableStreamWriter;
class JSDOMGlobalObject;
class WritableStream;
template<typename> class ExceptionOr;

class WritableStreamDefaultWriter : public RefCounted<WritableStreamDefaultWriter> {
public:
    static ExceptionOr<Ref<WritableStreamDefaultWriter>> create(JSDOMGlobalObject&, WritableStream&);

    virtual ~WritableStreamDefaultWriter();

    enum class Type : bool { Default, WebTransport };
    virtual Type type() const { return Type::Default; }

    ExceptionOr<std::optional<double>> desiredSize(JSDOMGlobalObject&);
    ExceptionOr<void> releaseLock(JSDOMGlobalObject&);

    InternalWritableStreamWriter& NODELETE internalWriter() { return m_internalWriter.get(); }

protected:
    explicit WritableStreamDefaultWriter(Ref<InternalWritableStreamWriter>&&);

private:
    const Ref<InternalWritableStreamWriter> m_internalWriter;
};

} // namespace WebCore
