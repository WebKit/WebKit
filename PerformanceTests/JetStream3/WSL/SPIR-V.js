/*
 * Copyright (C) 2017 Apple Inc. All rights reserved.
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
"use strict";

// This function accepts a JSON object describing the SPIR-V syntax.
// For example, https://github.com/KhronosGroup/SPIRV-Headers/blob/master/include/spirv/1.2/spirv.core.grammar.json
function SPIRV(json) {
    let result = {
        ops: {},
        kinds: {}
    }

    let composites = new Map();
    let ids = new Map();
    for (let kind of json.operand_kinds) {
        switch (kind.category) {
        case "BitEnum":
        case "ValueEnum":
            let enumerants = { category: kind.category };
            for (let enumerant of kind.enumerants) {
                enumerants[enumerant.enumerant] = enumerant;
            }
            result.kinds[kind.kind] = enumerants;
            break;
        case "Composite":
            composites.set(kind.kind, kind);
            break;
        case "Id":
            ids.set(kind.kind, kind);
            break;
        }
    }

    function matchType(operandInfoKind, operand) {
        switch (operandInfoKind) {
            // FIXME: I'm not actually sure that Ids should be unsigned.
            case "IdResultType":
            case "IdResult":
            case "IdRef":
            case "IdScope":
            case "IdMemorySemantics":
            case "LiteralExtInstInteger":
                if (typeof operand != "number")
                    throw new Error("Operand needs to be a number");
                if ((operand >>> 0) != operand)
                    throw new Error("Operand needs to fit in an unsigned int");
                return;
            case "LiteralInteger":
                if (typeof operand != "number")
                    throw new Error("Operand needs to be a number");
                if ((operand | 0) != operand)
                    throw new Error("Operand needs to fit in an int");
                return;
            case "LiteralString":
                if (typeof operand != "string")
                    throw new Error("Operand needs to be a string");
                return;
            case "LiteralContextDependentNumber":
            case "LiteralSpecConstantOpInteger":
                if (typeof operand != "number")
                    throw new Error("Operand needs to be a number");
                if ((operand >>> 0) != operand && (operand | 0) != operand)
                    throw new Error("Operand needs to fit in an unsigned int or an int.");
                return;
        }
        let kind = result.kinds[operandInfoKind];
        if (kind) {
            if (operand instanceof Array) {
                if (kind.category != "BitEnum")
                    throw new Error("Passing an array to a " + kind.category + " operand");
                for (let operandItem of operand) {
                    if (kind[operandItem.enumerant] != operandItem)
                        throw new Error("" + operandItem.enumerant + " is not a member of " + operandInfoKind);
                }
                return;
            }
            if (kind[operand.enumerant] != operand)
                throw new Error("" + operand.enumerant + " is not a member of " + operandInfoKind);
            return;
        }
        throw new Error("Unknown type: " + operandInfoKind);
    }

    class OperandChecker {
        constructor(operandInfos)
        {
            this._operandInfos = operandInfos || [];
            this._operandIndex = 0;
            this._operandInfoIndex = 0;
            this._parameters = [];
        }

        _isStar(operandInfo)
        {
            switch (operandInfo.kind) {
                case "LiteralContextDependentNumber":
                case "LiteralSpecConstantOpInteger":
                    // These types can be any width.
                    return true;
            }
            return operandInfo.quantifier && operandInfo.quantifier == "*";
        }

        nextComparisonType(operand)
        {
            if (this._operandInfoIndex >= this._operandInfos.length)
                throw new Error("Specified operand does not correspond to any that the instruction expects.");
            let operandInfo = this._operandInfos[this._operandInfoIndex];

            let isStar = this._isStar(operandInfo);

            if (this._parameters.length != 0) {
                let result = this._parameters[0];
                this._parameters.splice(0, 1);
                // FIXME: Handle parameters that require their own parameters
                ++this._operandIndex;
                if (this._parameters.length == 0 && !isStar)
                    ++this._operandInfoIndex;
                return result;
            }

            let composite = composites.get(operandInfo.kind);
            if (composite) {
                for (let base of composite.bases)
                    this._parameters.push(base);
                nextComparisonType(operand);
                return;
            }

            let kind = result.kinds[operandInfo.kind];
            if (kind) {
                let enumerant = kind[operand.enumerant];
                if (enumerant) {
                    let parameters = enumerant.parameters;
                    if (parameters) {
                        for (let parameter of parameters) {
                            this._parameters.push(parameter.kind);
                        }
                        ++this._operandIndex;
                        return operandInfo.kind;
                    }
                }
            }

            ++this._operandIndex;
            if (!isStar)
                ++this._operandInfoIndex;
            return operandInfo.kind;
        }

        check(operand)
        {
            matchType(this.nextComparisonType(operand), operand);
        }

        finalize()
        {
            if (this._parameters.length != 0)
                throw new Error("Operand not specified for parameter.");
            for (let i = this._operandInfoIndex; i < this._operandInfos.length; ++i) {
                let operandInfo = this._operandInfos[i];
                let quantifier = operandInfo.quantifier;
                if (quantifier != "?" && !this._isStar(operandInfo))
                    throw new Error("Did not specify operand " + i + " to instruction.");
            }
        }
    }

    for (let instruction of json.instructions) {
        if (!instruction.opname.startsWith("Op"))
            continue;
        let attributeName = instruction.opname.substring(2);
        result.ops[attributeName] = class {
            constructor(...operands)
            {
                let operandChecker = new OperandChecker(instruction.operands);
                for (let operand of operands)
                    operandChecker.check(operand);
                operandChecker.finalize();

                this._operands = operands;
            }
            get operands()
            {
                return this._operands;
            }
            get opname()
            {
                return instruction.opname;
            }
            get opcode()
            {
                return instruction.opcode;
            }
            get operandInfo()
            {
                return instruction.operands;
            }
            get storageSize()
            {
                let result = 1;
                for (let operand of this.operands) {
                    if (typeof operand == "number")
                        ++result;
                    else if (typeof operand == "string")
                        result += (((operand.length + 1) + 3) / 4) | 0;
                    else
                        ++result;
                }
                return result;
            }
            get largestId()
            {
                let maximumId = 0;
                let operandChecker = new OperandChecker(this.operandInfo);
                for (let operand of this.operands) {
                    let type = operandChecker.nextComparisonType(operand);
                    let idType = ids.get(type);
                    if (idType)
                        maximumId = Math.max(maximumId, operand);
                }
                return maximumId;
            }
        }
    }
    return result;
}

class SPIRVAssembler {
    constructor()
    {
        this._largestId = 0;
        this._size = 5;
        this._storage = new Uint32Array(this.size);
        this.storage[0] = 0x07230203; // Magic number
        this.storage[1] = 0x00010000; // Version: 1.0
        this.storage[2] = 0x574B0000; // Tool: "WK"
        this.storage[3] = 0; // Placeholder: All <id>s are less than this value.
        this.storage[4] = 0; // Reserved
    }

    append(op)
    {
        this._largestId = Math.max(this._largestId, op.largestId);

        let deltaStorageSize = op.storageSize;
        if (this.size + deltaStorageSize > this.storage.length) {
            let newStorageSize = (this.size + deltaStorageSize) * 1.5;
            let newStorage = new Uint32Array(newStorageSize);
            for (let i = 0; i < this.size; ++i)
                newStorage[i] = this.storage[i];
            this._storage = newStorage;
        }
        if ((deltaStorageSize & 0xFFFF) != deltaStorageSize || (op.opcode & 0xFFFF) != op.opcode)
            throw new Error("Out of bounds!");
        this.storage[this.size] = ((deltaStorageSize & 0xFFFF) << 16) | (op.opcode & 0xFFFF);
        ++this._size;
        if (deltaStorageSize <= op.operands.size)
            throw new Error("The storage size must be greater than the number of parameters");
        for (let i = 0; i < op.operands.length; ++i) {
            let operand = op.operands[i];
            if (typeof operand == "number") {
                this.storage[this.size] = operand;
                ++this._size;
            } else if (typeof operand == "string") {
                let word = 0;
                for (let j = 0; j < operand.length + 1; ++j) {
                    let charCode;
                    if (j < operand.length)
                        charCode = operand.charCodeAt(j);
                    else
                        charCode = 0;
                    if (charCode > 0xFF)
                        throw new Error("Non-ASCII strings don't work yet");
                    switch (j % 4) {
                    case 0:
                        word |= charCode;
                        break;
                    case 1:
                        word |= (charCode << 8);
                        break;
                    case 2:
                        word |= (charCode << 16);
                        break;
                    case 3:
                        word |= (charCode << 24);
                        this.storage[this.size + ((j / 4) | 0)] = word;
                        word = 0;
                        break;
                    }
                }
                if ((operand.length % 4) != 0)
                    this.storage[this.size + (((operand.length + 1) / 4) | 0)] = word;
                this._size += (((operand.length + 1) + 3) / 4) | 0;
            } else {
                this.storage[this.size] = operand.value;
                ++this._size;
            }
        }
    }

    get size()
    {
        return this._size;
    }

    get storage()
    {
        return this._storage;
    }

    get result()
    {
        this.storage[3] = this._largestId + 1;
        return this.storage.slice(0, this.size);
    }
}
