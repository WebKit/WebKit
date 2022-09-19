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

#include "config.h"
#include "JSReportBody.h"

#include "COEPInheritenceViolationReportBody.h"
#include "CORPViolationReportBody.h"
#include "CSPViolationReportBody.h"
#include "DeprecationReportBody.h"
#include "JSCOEPInheritenceViolationReportBody.h"
#include "JSCORPViolationReportBody.h"
#include "JSCSPViolationReportBody.h"
#include "JSDOMBinding.h"
#include "JSDeprecationReportBody.h"
#include "JSTestReportBody.h"
#include "ReportBody.h"
#include "TestReportBody.h"
#include "ViolationReportType.h"

namespace WebCore {
using namespace JSC;

JSValue toJSNewlyCreated(JSC::JSGlobalObject*, JSDOMGlobalObject* globalObject, Ref<ReportBody>&& reportBody)
{
    if (is<CSPViolationReportBody>(reportBody))
        return createWrapper<CSPViolationReportBody>(globalObject, WTFMove(reportBody));
    if (is<COEPInheritenceViolationReportBody>(reportBody))
        return createWrapper<COEPInheritenceViolationReportBody>(globalObject, WTFMove(reportBody));
    if (is<CORPViolationReportBody>(reportBody))
        return createWrapper<CORPViolationReportBody>(globalObject, WTFMove(reportBody));
    if (is<DeprecationReportBody>(reportBody))
        return createWrapper<DeprecationReportBody>(globalObject, WTFMove(reportBody));
    if (is<TestReportBody>(reportBody))
        return createWrapper<TestReportBody>(globalObject, WTFMove(reportBody));
    return createWrapper<ReportBody>(globalObject, WTFMove(reportBody));
}

JSValue toJS(JSGlobalObject* lexicalGlobalObject, JSDOMGlobalObject* globalObject, ReportBody& reportBody)
{
    return wrap(lexicalGlobalObject, globalObject, reportBody);
}

JSValue JSReportBody::toJSON(JSGlobalObject& lexicalGlobalObject, CallFrame& callFrame)
{
    UNUSED_PARAM(lexicalGlobalObject);
    UNUSED_PARAM(callFrame);

    return jsUndefined();
}

} // namepace WebCore
