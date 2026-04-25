/*
 * Copyright (C) 2025 Sosuke Suzuki <aosukeke@gmail.com>.
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
#include "SuppressedError.h"

#include "ExceptionScope.h"
#include "JSCJSValueInlines.h"
#include "JSGlobalObjectInlines.h"

namespace JSC {

ErrorInstance* createSuppressedError(JSGlobalObject* globalObject, VM& vm, Structure* structure, JSValue error, JSValue suppressed, JSValue message, ErrorInstance::SourceAppender appender, RuntimeType type, bool useCurrentFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    String messageString = message.isUndefined() ? String() : message.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    auto* suppressedError = ErrorInstance::create(vm, structure, messageString, JSValue(), appender, type, ErrorType::SuppressedError, useCurrentFrame);

    suppressedError->putDirect(vm, vm.propertyNames->error, error, static_cast<unsigned>(PropertyAttribute::DontEnum));
    RETURN_IF_EXCEPTION(scope, nullptr);

    suppressedError->putDirect(vm, vm.propertyNames->suppressed, suppressed, static_cast<unsigned>(PropertyAttribute::DontEnum));
    RETURN_IF_EXCEPTION(scope, nullptr);

    return suppressedError;
}

} // namespace JSC
