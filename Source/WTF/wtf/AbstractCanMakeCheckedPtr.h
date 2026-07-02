/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#include <cstdint>

namespace WTF {

// Use this class when an abstract base class needs CheckedPtr/CheckedRef support, and the
// CanMakeCheckedPtr implementation will be in a concrete subclass.
class AbstractCanMakeCheckedPtr {
public:
    virtual uint32_t checkedPtrCount() const = 0;
    virtual uint32_t checkedPtrCountWithoutThreadCheck() const = 0;
    virtual void incrementCheckedPtrCount() const = 0;
    virtual void decrementCheckedPtrCount() const = 0;
    virtual void setDidBeginCheckedPtrDeletion() = 0;

protected:
    virtual ~AbstractCanMakeCheckedPtr() = default;
};

} // namespace WTF

#define OVERRIDE_ABSTRACT_CAN_MAKE_CHECKEDPTR(BaseClass) \
    uint32_t checkedPtrCount() const final { return BaseClass::checkedPtrCount(); } \
    uint32_t checkedPtrCountWithoutThreadCheck() const final { return BaseClass::checkedPtrCountWithoutThreadCheck(); } \
    void incrementCheckedPtrCount() const final { BaseClass::incrementCheckedPtrCount(); } \
    void decrementCheckedPtrCount() const final { BaseClass::decrementCheckedPtrCount(); } \
    void setDidBeginCheckedPtrDeletion() final { BaseClass::setDidBeginCheckedPtrDeletion(); } \
    using __unused_for_semicolon_canmakecheckedptr = int

using WTF::AbstractCanMakeCheckedPtr;
