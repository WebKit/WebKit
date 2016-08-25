/*
 * Copyright (C) 2012 Google Inc. All rights reserved.
 * Copyright (C) 2012 Intel Inc. All rights reserved.
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

#ifndef PerformanceResourceTiming_h
#define PerformanceResourceTiming_h

#if ENABLE(WEB_TIMING)

#include "LoadTiming.h"
#include "NetworkLoadTiming.h"
#include "PerformanceEntry.h"
#include <wtf/RefPtr.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

class Document;
class URL;
class NetworkLoadTiming;
class ResourceRequest;
class ResourceResponse;

class PerformanceResourceTiming final : public PerformanceEntry {
public:
    static Ref<PerformanceResourceTiming> create(const AtomicString& initiatorType, const URL& originalURL, const ResourceResponse& response, LoadTiming loadTiming, Document* requestingDocument)
    {
        return adoptRef(*new PerformanceResourceTiming(initiatorType, originalURL, response, loadTiming, requestingDocument));
    }

    AtomicString initiatorType() const;

    double redirectStart() const;
    double redirectEnd() const;
    double fetchStart() const;
    double domainLookupStart() const;
    double domainLookupEnd() const;
    double connectStart() const;
    double connectEnd() const;
    double secureConnectionStart() const;
    double requestStart() const;
    double responseStart() const;
    double responseEnd() const;

    bool isResource() const override { return true; }

private:
    PerformanceResourceTiming(const AtomicString& initatorType, const URL& originalURL, const ResourceResponse&, LoadTiming, Document*);
    ~PerformanceResourceTiming();

    double resourceTimeToDocumentMilliseconds(double deltaMilliseconds) const;

    AtomicString m_initiatorType;
    NetworkLoadTiming m_timing;
    LoadTiming m_loadTiming;
    bool m_shouldReportDetails;
    RefPtr<Document> m_requestingDocument;
};

}

SPECIALIZE_TYPE_TRAITS_BEGIN(WebCore::PerformanceResourceTiming)
    static bool isType(const WebCore::PerformanceEntry& entry) { return entry.isResource(); }
SPECIALIZE_TYPE_TRAITS_END()

#endif // ENABLE(WEB_TIMING)
#endif // !defined(PerformanceResourceTiming_h)
