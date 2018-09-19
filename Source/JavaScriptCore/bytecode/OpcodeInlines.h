/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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

#pragma once

#include "ArrayProfile.h"
#include "Instruction.h"
#include "InterpreterInlines.h"
#include "Opcode.h"

namespace JSC {

enum OpcodeShape {
    AnyOpcodeShape,
    OpCallShape,
    OpHasIndexedPropertyShape,
    OpGetArrayLengthShape,
    OpGetByValShape,
    OpInByValShape,
    OpPutByValShape,
};

template<OpcodeShape shape, typename = std::enable_if_t<shape != AnyOpcodeShape>>
inline bool isOpcodeShape(OpcodeID opcodeID)
{
    if (shape == OpCallShape) {
        static_assert(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_tail_call), "");
        static_assert(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_call_eval), "");
        static_assert(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_call_varargs), "");
        static_assert(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_tail_call_varargs), "");
        static_assert(OPCODE_LENGTH(op_call) == OPCODE_LENGTH(op_tail_call_forward_arguments), "");
        return opcodeID == op_call
            || opcodeID == op_tail_call
            || opcodeID == op_call_eval
            || opcodeID == op_call_varargs
            || opcodeID == op_tail_call_varargs
            || opcodeID == op_tail_call_forward_arguments;
    }

    if (shape == OpHasIndexedPropertyShape)
        return opcodeID == op_has_indexed_property;

    if (shape == OpGetArrayLengthShape)
        return opcodeID == op_get_array_length;

    if (shape == OpGetByValShape)
        return opcodeID == op_get_by_val;

    if (shape == OpInByValShape)
        return opcodeID == op_in_by_val;

    if (shape == OpPutByValShape) {
        static_assert(OPCODE_LENGTH(op_put_by_val) == OPCODE_LENGTH(op_put_by_val_direct), "");
        return opcodeID == op_put_by_val
            || opcodeID == op_put_by_val_direct;
    }

    RELEASE_ASSERT_NOT_REACHED();
}

template<OpcodeShape shape, typename = std::enable_if_t<shape != AnyOpcodeShape>>
inline bool isOpcodeShape(const Instruction* instruction)
{
    OpcodeID opcodeID = Interpreter::getOpcodeID(*instruction);
    return isOpcodeShape<shape>(opcodeID);
}

template<OpcodeShape shape = AnyOpcodeShape>
inline ArrayProfile* arrayProfileFor(const Instruction* instruction)
{
    ArrayProfile* arrayProfile = nullptr;
    OpcodeID opcodeID = Interpreter::getOpcodeID(*instruction);
    if (OpCallShape == shape || (AnyOpcodeShape == shape && isOpcodeShape<OpCallShape>(opcodeID))) {
        ASSERT(isOpcodeShape<OpCallShape>(instruction));
        arrayProfile = instruction[OPCODE_LENGTH(op_call) - 2].u.arrayProfile;

    } else if (OpHasIndexedPropertyShape == shape || (AnyOpcodeShape == shape && isOpcodeShape<OpHasIndexedPropertyShape>(opcodeID))) {
        ASSERT(isOpcodeShape<OpHasIndexedPropertyShape>(instruction));
        arrayProfile = instruction[4].u.arrayProfile;

    } else if (OpGetArrayLengthShape == shape || (AnyOpcodeShape == shape && isOpcodeShape<OpGetArrayLengthShape>(opcodeID))) {
        ASSERT(isOpcodeShape<OpGetArrayLengthShape>(instruction));
        arrayProfile = instruction[4].u.arrayProfile;

    } else if (OpGetByValShape == shape || (AnyOpcodeShape == shape && isOpcodeShape<OpGetByValShape>(opcodeID))) {
        ASSERT(isOpcodeShape<OpGetByValShape>(instruction));
        arrayProfile = instruction[4].u.arrayProfile;

    } else if (OpInByValShape == shape || (AnyOpcodeShape == shape && isOpcodeShape<OpInByValShape>(opcodeID))) {
        ASSERT(isOpcodeShape<OpInByValShape>(instruction));
        arrayProfile = instruction[OPCODE_LENGTH(op_in_by_val) - 1].u.arrayProfile;

    } else if (OpPutByValShape == shape || (AnyOpcodeShape == shape && isOpcodeShape<OpPutByValShape>(opcodeID))) {
        ASSERT(isOpcodeShape<OpPutByValShape>(instruction));
        arrayProfile = instruction[4].u.arrayProfile;

    } else if (AnyOpcodeShape != shape)
        RELEASE_ASSERT_NOT_REACHED();

    ASSERT(!arrayProfile || arrayProfile->isValid());
    return arrayProfile;
}

} // namespace JSC
