/*
 * Copyright (C) 2017-2018 Apple Inc. All rights reserved.
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
#include "MoveOnlyLifecycleLogger.h"

namespace TestWebKitAPI {

MoveOnlyLifecycleLogger::MoveOnlyLifecycleLogger()
{
    log() << "construct(" << name << ") ";
}

MoveOnlyLifecycleLogger::MoveOnlyLifecycleLogger(const char* name)
    : name { name }
{
    log() << "construct(" << name << ") ";
}

MoveOnlyLifecycleLogger::~MoveOnlyLifecycleLogger()
{
    log() << "destruct(" << name << ") ";
}

MoveOnlyLifecycleLogger::MoveOnlyLifecycleLogger(MoveOnlyLifecycleLogger&& other)
{
    std::swap(name, other.name);
    log() << "move-construct(" << name << ") ";
}

MoveOnlyLifecycleLogger& MoveOnlyLifecycleLogger::operator=(MoveOnlyLifecycleLogger&& other)
{
    std::swap(name, other.name);
    log() << "move-assign(" << name << ") ";
    return *this;
}

void MoveOnlyLifecycleLogger::setName(const char* newName)
{
    name = newName;
    log() << "set-name(" << name << ") ";
}

TEST(MoveOnlyLifecycleLogger, Basic)
{
    { MoveOnlyLifecycleLogger l; }
    ASSERT_STREQ("construct(<default>) destruct(<default>) ", takeLogStr().c_str());

    { MoveOnlyLifecycleLogger l("a"); }
    ASSERT_STREQ("construct(a) destruct(a) ", takeLogStr().c_str());

    { MoveOnlyLifecycleLogger l("d"); MoveOnlyLifecycleLogger l2(WTFMove(l)); }
    ASSERT_STREQ("construct(d) move-construct(d) destruct(d) destruct(<default>) ", takeLogStr().c_str());

    { MoveOnlyLifecycleLogger l("e"); MoveOnlyLifecycleLogger l2; l2 = WTFMove(l); }
    ASSERT_STREQ("construct(e) construct(<default>) move-assign(e) destruct(e) destruct(<default>) ", takeLogStr().c_str());

    { MoveOnlyLifecycleLogger l("f"); l.setName("x"); }
    ASSERT_STREQ("construct(f) set-name(x) destruct(x) ", takeLogStr().c_str());

    { static MoveOnlyLifecycleLogger l("g"); }
    ASSERT_STREQ("construct(g) ", takeLogStr().c_str());
}

}
