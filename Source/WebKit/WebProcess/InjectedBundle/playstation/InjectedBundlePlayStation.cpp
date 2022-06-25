/*
 * Copyright (C) 2020 Sony Interactive Entertainment Inc.
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
#include "InjectedBundle.h"

#include "WKBundleAPICast.h"
#include "WKBundleInitialize.h"
#include "library-bundle.h"

namespace WebKit {

bool InjectedBundle::initialize(const WebProcessCreationParameters& parameters, API::Object* initializationUserData)
{
    auto bundle = LibraryBundle::create(m_path.utf8().data());
    m_platformBundle = bundle;
    if (!m_platformBundle) {
        printf("PlayStation::Bundle::create failed\n");
        return false;
    }
    WKBundleInitializeFunctionPtr initializeFunction = reinterpret_cast<WKBundleInitializeFunctionPtr>(bundle->resolve("WKBundleInitialize"));
    if (!initializeFunction) {
        printf("PlayStation::Bundle::resolve failed\n");
        return false;
    }
    initializeFunction(toAPI(this), toAPI(initializationUserData));
    return true;
}

void InjectedBundle::setBundleParameter(WTF::String const&, IPC::DataReference const&)
{

}

void InjectedBundle::setBundleParameters(const IPC::DataReference&)
{
}

} // namespace WebKit
