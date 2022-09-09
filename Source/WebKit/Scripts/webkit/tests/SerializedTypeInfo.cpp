/*
 * Copyright (C) 2022 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
 * ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"
#include "SerializedTypeInfo.h"

#if ENABLE(IPC_TESTING_API)

namespace WebKit {

Vector<SerializedTypeInfo> allSerializedTypes()
{
    return {
        { "Namespace::Subnamespace::StructName"_s, {
            "FirstMemberType"_s,
            "SecondMemberType"_s,
            "RetainPtr<CFTypeRef>"_s,
        } },
        { "Namespace::OtherClass"_s, {
            "bool"_s,
            "int"_s,
            "bool"_s,
        } },
        { "Namespace::ReturnRefClass"_s, {
            "double"_s,
            "double"_s,
            "std::unique_ptr<int>"_s,
        } },
        { "Namespace::EmptyConstructorStruct"_s, {
            "int"_s,
            "double"_s,
        } },
        { "Namespace::EmptyConstructorNullable"_s, {
            "bool"_s,
            "MemberType"_s,
            "OtherMemberType"_s,
        } },
        { "WithoutNamespace"_s, {
            "int"_s,
        } },
        { "WithoutNamespaceWithAttributes"_s, {
            "int"_s,
        } },
    };
}

} // namespace WebKit

#endif // ENABLE(IPC_TESTING_API)
