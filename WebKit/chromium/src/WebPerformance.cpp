/*
 * Copyright (C) 2010 Google Inc. All rights reserved.
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

#include "config.h"
#include "WebPerformance.h"

#include "Performance.h"

using namespace WebCore;

namespace WebKit {

void WebPerformance::reset()
{
    m_private.reset();
}

void WebPerformance::assign(const WebPerformance& other)
{
    m_private = other.m_private;
}

WebNavigationType WebPerformance::navigationType() const
{
    switch (m_private->navigation()->type()) {
    case Navigation::NAVIGATE:
        return WebNavigationTypeOther;
    case Navigation::RELOAD:
        return WebNavigationTypeReload;
    case Navigation::BACK_FORWARD:
        return WebNavigationTypeBackForward;
    }
    ASSERT_NOT_REACHED();
    return WebNavigationTypeOther;
}

double WebPerformance::navigationStart() const
{
    return static_cast<double>(m_private->timing()->navigationStart());
}

double WebPerformance::unloadEventEnd() const
{
    return static_cast<double>(m_private->timing()->unloadEventEnd());
}

double WebPerformance::redirectStart() const
{
    return static_cast<double>(m_private->timing()->redirectStart());
}

double WebPerformance::redirectEnd() const
{
    return static_cast<double>(m_private->timing()->redirectEnd());
}

unsigned short WebPerformance::redirectCount() const
{
    return m_private->navigation()->redirectCount();
}

double WebPerformance::fetchStart() const
{
    return static_cast<double>(m_private->timing()->fetchStart());
}

double WebPerformance::domainLookupStart() const
{
    return static_cast<double>(m_private->timing()->domainLookupStart());
}

double WebPerformance::domainLookupEnd() const
{
    return static_cast<double>(m_private->timing()->domainLookupEnd());
}

double WebPerformance::connectStart() const
{
    return static_cast<double>(m_private->timing()->connectStart());
}

double WebPerformance::connectEnd() const
{
    return static_cast<double>(m_private->timing()->connectEnd());
}

double WebPerformance::requestStart() const
{
    return static_cast<double>(m_private->timing()->requestStart());
}

double WebPerformance::requestEnd() const
{
    return static_cast<double>(m_private->timing()->requestEnd());
}

double WebPerformance::responseStart() const
{
    return static_cast<double>(m_private->timing()->responseStart());
}

double WebPerformance::responseEnd() const
{
    return static_cast<double>(m_private->timing()->responseEnd());
}

double WebPerformance::loadEventStart() const
{
    return static_cast<double>(m_private->timing()->loadEventStart());
}

double WebPerformance::loadEventEnd() const
{
    return static_cast<double>(m_private->timing()->loadEventEnd());
}

WebPerformance::WebPerformance(const PassRefPtr<Performance>& performance)
    : m_private(performance)
{
}

WebPerformance& WebPerformance::operator=(const PassRefPtr<Performance>& performance)
{
    m_private = performance;
    return *this;
}

WebPerformance::operator PassRefPtr<Performance>() const
{
    return m_private.get();
}

} // namespace WebKit
