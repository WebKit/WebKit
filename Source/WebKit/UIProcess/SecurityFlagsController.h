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

#include "SecurityFlags.h"
#include <wtf/Forward.h>
#include <wtf/NeverDestroyed.h>
#include <wtf/Noncopyable.h>
#include <wtf/Vector.h>
#include <wtf/text/WTFString.h>

namespace WebKit {

// The UIProcess's owner of the value securityFlags() answers, because it comes from a single device-global list.
// Setting the flags is main run loop only, because propagateToChildProcesses() touches the proxies.
class SecurityFlagsController {
    WTF_MAKE_NONCOPYABLE(SecurityFlagsController);
public:
    static SecurityFlagsController& singleton();

    // Applies the names from the user default. Called while the UIProcess initializes; does nothing after that.
    static void initialize();

    // The list is the whole truth: every flag it does not name is enforced, so an empty list resets everything.
    // Flags named by the user default stay disabled regardless.
    void setDisabledFlagsNamed(const Vector<String>&);

    // Ignores the user default, so a test gets the state it asked for on a device that has one set.
    void setDisabledFlagsNamedForTesting(const Vector<String>&);

private:
    friend class WTF::NeverDestroyed<SecurityFlagsController>;
    SecurityFlagsController();

    enum class IncludePersistentNames : bool { No, Yes };
    SecurityFlags::Storage flagsWithNamesDisabled(const Vector<String>&, IncludePersistentNames) const;
    void apply(SecurityFlags::Storage&&);
    void propagateToChildProcesses();

    static Vector<String> platformPersistentlyDisabledFlagNames();

    Vector<String> m_persistentlyDisabledNames;
};

} // namespace WebKit
