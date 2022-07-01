/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
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

#include "ActiveDOMObject.h"
#include "Document.h"
#include "Event.h"
#include "EventTarget.h"
#include "ExceptionOr.h"
#include "Formats.h"
#include "ReadableStream.h"
#include "ReadableStreamSource.h"
#include "ScriptExecutionContext.h"
#include "WritableStream.h"

#include <wtf/IsoMalloc.h>
#include <wtf/IsoMallocInlines.h>
#include <wtf/RefCounted.h>
#include <wtf/text/WTFString.h>

namespace JSC {
class CallFrame;
class JSGlobalObject;
class JSValue;
}

namespace WebCore {

class CompressionStream : public RefCounted<CompressionStream>,
    public ActiveDOMObject,
    public EventTargetWithInlineData {
    WTF_MAKE_ISO_ALLOCATED(CompressionStream);

public:
    ~CompressionStream() { }

    using RefCounted<CompressionStream>::ref;
    using RefCounted<CompressionStream>::deref;
    
    // Initialize readable and writable streams
    ExceptionOr<void> createStreams();
    
    // Initialize Compression Streams and perform initial basic error handling
    static ExceptionOr<Ref<CompressionStream>> create(Document& document, String string)
    {
        // gzip, deflate, and deflate-raw are the supported compression formats.
        if (string == String("gzip", 4))
            return adoptRef(*new CompressionStream(document, string, Formats::Gzip));
        if (string == String("deflate", 7))
            return adoptRef(*new CompressionStream(document, string, Formats::Deflate));
        if (string == String("deflate-raw", 11))
            return adoptRef(*new CompressionStream(document, string, Formats::Deflate_raw));

        // Section 5
        // 1. If format is unsupported in CompressionStream, then throw a TypeError.
        return Exception(TypeError, "Please provide a supported compression algorithm ('gzip', 'deflate', or 'deflate-raw')."_s);
    }
    
    ExceptionOr<RefPtr<ReadableStream>> readable();
    ExceptionOr<RefPtr<WritableStream>> writable();

    Formats::CompressionFormat getCompressionFormat() { return m_format; }

private:
    CompressionStream(Document& document, String string, Formats::CompressionFormat format) 
        : ActiveDOMObject(document), m_string(string), m_format(format) 
    {
        createStreams();
    }

    // Compression Format
    const String m_string;
    const Formats::CompressionFormat m_format;
    
    // Streams
    RefPtr<ReadableStream> m_readable;
    RefPtr<WritableStream> m_writable;
    RefPtr<SimpleReadableStreamSource> m_readableStreamSource;

    // ActiveDOMObject
    const char* activeDOMObjectName() const override { return "CompressionStream"; }
    void stop() override { }
    void suspend(ReasonForSuspension) override { }
    bool virtualHasPendingActivity() const final { return false; }

    // EventTargetWithInlineData
    EventTargetInterface eventTargetInterface() const override { return CompressionStreamEventTargetInterfaceType; }
    ScriptExecutionContext* scriptExecutionContext() const override { return ActiveDOMObject::scriptExecutionContext(); }
    void refEventTarget() override { ref(); }
    void derefEventTarget() override { deref(); }
};
}
