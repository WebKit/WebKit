/*
 * Copyright (C) 2021 Apple Inc. All rights reserved.
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
#include "Permissions.h"

#include "Exception.h"
#include "Frame.h"
#include "JSDOMPromiseDeferred.h"
#include "JSPermissionDescriptor.h"
#include "JSPermissionStatus.h"
#include "Navigator.h"
#include "Page.h"
#include "PermissionController.h"
#include "PermissionDescriptor.h"
#include <wtf/IsoMallocInlines.h>

namespace WebCore {

WTF_MAKE_ISO_ALLOCATED_IMPL(Permissions);

Ref<Permissions> Permissions::create(Navigator& navigator)
{
    return adoptRef(*new Permissions(navigator));
}

Permissions::Permissions(Navigator& navigator)
    : m_navigator(makeWeakPtr(navigator))
{
    if (auto context = navigator.scriptExecutionContext())
        m_controller = context->permissionController();
}

Navigator* Permissions::navigator()
{
    return m_navigator.get();
}

void Permissions::query(JSC::Strong<JSC::JSObject> permissionDescriptorValue, DOMPromiseDeferred<IDLInterface<PermissionStatus>>&& promise)
{
    // FIXME: support permissions in WorkerNavigator.
    if (!m_controller) {
        promise.reject(Exception { NotSupportedError });
        return;
    }

    auto context = m_navigator ? m_navigator->scriptExecutionContext() : nullptr;
    if (!context || !context->globalObject()) {
        promise.reject(Exception { InvalidStateError, "The context is invalid"_s });
        return;
    }

    if (!permissionDescriptorValue) {
        promise.reject(Exception { DataError, "The parameter is invalid"_s });
        return;
    }

    JSC::VM& vm = context->globalObject()->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);
    auto parameterDescriptor = convert<IDLDictionary<PermissionDescriptor>>(*context->globalObject(), permissionDescriptorValue.get());
    if (UNLIKELY(scope.exception())) {
        promise.reject(Exception { ExistingExceptionError });
        return;
    }

    auto* origin = context->securityOrigin();
    auto originData = origin ? origin->data() : SecurityOriginData { };
    auto permissionState = m_controller->query(ClientOrigin { context->topOrigin().data(), originData }, PermissionDescriptor { parameterDescriptor });
    promise.resolve(PermissionStatus::create(*context, permissionState, parameterDescriptor));
}

} // namespace WebCore
