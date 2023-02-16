/*
 * Copyright (c) 2023 Apple Inc. All rights reserved.
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
#include "Types.h"

#include <wtf/StringPrintStream.h>

namespace WGSL {

using namespace Types;

void Type::dump(PrintStream& out) const
{
    WTF::switchOn(*this,
        [&](const Primitive& primitive) {
            switch (primitive.kind) {
#define PRIMITIVE_CASE(kind, name) \
            case Primitive::kind: \
                out.print(name); \
                break;
            FOR_EACH_PRIMITIVE_TYPE(PRIMITIVE_CASE)
#undef PRIMITIVE_CASE
            }
        },
        [&](const Vector& vector) {
            out.print("vec", vector.size, "<", *vector.element, ">");
        },
        [&](const Matrix& matrix) {
            out.print("mat", matrix.columns, "x", matrix.rows, "<", *matrix.element, ">");
        },
        [&](const Array& array) {
            out.print("array<", *array.element);
            if (array.size.has_value())
                out.print(", ", *array.size);
            out.print(">");
        },
        [&](const Struct& structure) {
            out.print(structure.name);
        },
        [&](const Function&) {
            // FIXME: implement this
            ASSERT_NOT_REACHED();
        },
        [&](const Bottom&) {
            // Bottom is an implementation detail and should never leak, but we
            // keep the ability to print it in debug to help when dumping types
            out.print("⊥");
        });
}

String Type::toString() const
{
    StringPrintStream out;
    dump(out);
    return out.toString();
}

} // namespace WGSL
