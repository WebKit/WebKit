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

#include "config.h"
#include "ReportingObserver.h"

#include "Report.h"
#include "ReportingObserverCallback.h"
#include "ScriptExecutionContext.h"
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebCore {

Ref<ReportingObserver> ReportingObserver::create(ScriptExecutionContext& context, ReportingObserverCallback& callback, const Options& options)
{
    return adoptRef(*new ReportingObserver(context, callback, options));
}

ReportingObserver::ReportingObserver(ScriptExecutionContext& context, ReportingObserverCallback& callback, const ReportingObserver::Options& options)
{
    UNUSED_PARAM(context);
    UNUSED_PARAM(callback);
    UNUSED_PARAM(options);
}

ReportingObserver::~ReportingObserver()
{
    disconnect();
}

void ReportingObserver::disconnect()
{
}

void ReportingObserver::observe()
{
}

Vector<Report> ReportingObserver::takeRecords()
{
    return { };
}

} // namespace WebCore
