/*
 * Copyright (C) 2016-2017 Apple Inc. All rights reserved.
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

/*
 * Base utility types for the AST.
 *
 * Valid methods for Node:
 *
 * node.children -> Returns an array of immediate children.  It has been modified 
 *     from the original Ruby version to dump directly nearly the original source.
 *
 * node.descendents -> Returns an array of all strict descendants (children
 *     and children of children, transitively).
 *
 */

class Node
{
    constructor(codeOrigin)
    {
        this._codeOrigin = codeOrigin;
    }

    get codeOriginString()
    {
        return this._codeOrigin.toString();
    }

    get codeOrigin()
    {
        return this._codeOrigin;
    }
}

class NoChildren extends Node
{
    constructor(codeOrigin)
    {
        super(codeOrigin);
    }

    children()
    {
        return [];
    }
}

function structOffsetKey(struct, field)
{
    return struct + "::" + field;
}

// AST nodes.

class StructOffset extends NoChildren
{
    constructor(codeOrigin, struct, field)
    {
        super(codeOrigin);
        this._struct = struct;
        this._field = field;
    }

    static forField(codeOrigin, struct, field)
    {
        let key = structOffsetKey(struct, field);

        if (!this.mapping[key])
            this.mapping[key] = new StructOffset(codeOrigin, struct, field);

        return this.mapping[key];
    }

    static resetMappings()
    {
        this.mapping = {};
    }

    dump()
    {
        return this.struct + "::" + this.field;
    }

    compare(other)
    {
        if (this.struct != other.struct)
            return this.struct.localeCompare(other.struct);

        return this.field.localeCompare(other.field);
    }

    get struct()
    {
        return this._struct;
    }

    get field()
    {
        return this._field;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

StructOffset.mapping = {};

class Sizeof extends NoChildren
{
    constructor(codeOrigin, struct)
    {
        super(codeOrigin);
        this._struct = struct;
    }

    static forName(codeOrigin, struct)
    {
        if (!this.mapping[struct])
            this.mapping[struct] = new Sizeof(codeOrigin, struct);

        return this.mapping[struct];
    }

    static resetMappings()
    {
        this.mapping = {};
    }

    dump()
    {
        return "sizeof " + this.struct;
    }

    compare(other)
    {
        return this.struct.localeCompare(other.struct);
    }

    get struct()
    {
        return this._struct;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

Sizeof.mapping = {};

class Immediate extends NoChildren
{
    constructor(codeOrigin, value)
    {
        super(codeOrigin);
        if (value instanceof Number)
            value = value.valueOf();
        this._value = value;
        if (typeof(value) != "number")
            throw "Bad immediate value " + value.inspect() + " at " + this.codeOriginString();
    }

    dump()
    {
        return this.value.toString();
    }

    equals(other)
    {
        return other instanceof Immediate && other.value == this.value;
    }

    get value()
    {
        return this._value;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

class AddImmediates extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " + " + this.right.dump() + ")";
    }

    value()
    {
        return (this.left.value() + this.right.value()).toString();
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

class SubImmediates extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " - " + this.right.dump() + ")";
    }

    value()
    {
        return (this.left.value() - this.right.value()).toString();
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

class MulImmediates extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " * " + this.right.dump() + ")";
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class NegImmediate extends Node
{
    constructor(codeOrigin, child)
    {
        super(codeOrigin);
        this._child = child;
    }

    children()
    {
        return [this.child];
    }

    dump()
    {
        return "(-" + this.child.dump() + ")";
    }

    get child()
    {
        return this._child;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class OrImmediates extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " | " + this.right.dump() + ")";
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class AndImmediates extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " & " + this.right.dump() + ")";
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class XorImmediates extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " ^ " + this.right.dump() + ")";
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class BitnotImmediate extends Node
{
    constructor(codeOrigin, child)
    {
        super(codeOrigin);
        this._child = child;
    }

    children()
    {
        return [this.child];
    }

    dump()
    {
        return "(~" + this.child.dump() + ")";
    }

    get child()
    {
        return this._child;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return true;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class StringLiteral extends NoChildren
{
    constructor(codeOrigin, value)
    {
        super(codeOrigin);
        if (!value instanceof String || value[0] != "\"" || value.slice(-1) != "\"")
            throw "Bad string literal " + value.inspect() + " at " + this.codeOriginString();
        this._value = value.slice(1, -1);
    }

    dump()
    {
        return "\"" + this.value + "\"";
    }

    equals(other)
    {
        return other instanceof StringLiteral && other.value == this.value;
    }

    get value()
    {
        return this._value;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class RegisterID extends NoChildren
{
    constructor(codeOrigin, name)
    {
        super(codeOrigin);
        this._name = name;
    }

    static forName(codeOrigin, name)
    {
        if (!this.mapping[name])
            this.mapping[name] = new RegisterID(codeOrigin, name);

        return this.mapping[name];
    }

    dump()
    {
        return this.name;
    }

    get name()
    {
        return this._name;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isRegister()
    {
        return true;
    }
}

RegisterID.mapping = {};

class FPRegisterID extends NoChildren
{
    constructor(codeOrigin, name)
    {
        super(codeOrigin);
        this._name = name;
    }

    static forName(codeOrigin, name)
    {
        if (!this.mapping[name])
            this.mapping[name] = new FPRegisterID(codeOrigin, name);

        return this.mapping[name];
    }

    dump()
    {
        return this.name;
    }

    get name()
    {
        return this._name;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return true;
    }
}

FPRegisterID.mapping = {};

class SpecialRegister extends NoChildren
{
    constructor(name)
    {
        this._name = name;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return true;
    }
}

class Variable extends NoChildren
{
    constructor(codeOrigin, name)
    {
        super(codeOrigin);
        this._name = name;
    }

    static forName(codeOrigin, name)
    {
        if (!this.mapping[name])
            this.mapping[name] = new Variable(codeOrigin, name);

        return this.mapping[name];
    }

    dump()
    {
        return this.name;
    }

    get name()
    {
        return this._name;
    }

    inspect()
    {
        return "<variable " + this.name + " at " + this.codeOriginString();
    }
}

Variable.mapping = {};

class Address extends Node
{
    constructor(codeOrigin, base, offset)
    {
        super(codeOrigin);
        this._base = base;
        this._offset = offset;
        if (!base instanceof Variable && !base.register)
            throw "Bad base for address " + base.inspect() + " at " + this.codeOriginString();
        if (!offset instanceof Variable && !offset.immediate())
            throw "Bad offset for address " + offset.inspect() + " at " + this.codeOriginString();
    }

    withOffset(extraOffset)
    {
        return new Address(this.codeOrigin, this.base, new Immediate(this.codeOrigin, this.offset.value + extraOffset));
    }

    children()
    {
        return [this.base, this.offset];
    }

    dump()
    {
        return this.offset.dump() + "[" + this.base.dump() + "]";
    }

    get base()
    {
        return this._base;
    }

    get offset()
    {
        return this._offset;
    }

    get isAddress()
    {
        return true;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

class BaseIndex extends Node
{
    constructor(codeOrigin, base, index, scale, offset)
    {
        super(codeOrigin);
        this._base = base;
        this._index = index;
        this._scale = scale;
        if (![1, 2, 4, 8].includes(this._scale))
            throw "Bad scale " + this._scale + " at " + this.codeOriginString();
        this._offset = offset;
    }

    scaleShift()
    {
        switch(this.scale)
        {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        case 8:
            return 3;
        default:
            throw "Bad scale " + this.scale + " at " + this.codeOriginString();
        }
    }

    withOffset(extraOffset)
    {
        return new BaseIndex(codeOrigin, this.base, this.index, this.scale, new Immediate(codeOrigin, this.offset.value + extraOffset));
    }

    children()
    {
        return [this.base, this.index, this.offset];
    }

    dump()
    {
        return this.offset.dump() + "[" + this.base.dump() + ", " + this.index.dump() + ", " + this.scale + "]";
    }

    get base()
    {
        return this._base;
    }

    get index()
    {
        return this._index;
    }

    get scale()
    {
        return this._scale;
    }

    get offset()
    {
        return this._offset;
    }

    get isAddress()
    {
        return true;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return false;
    }

    get isRegister()
    {
        return false;
    }
}

class AbsoluteAddress extends NoChildren
{
    constructor(codeOrigin, address)
    {
        super(codeOrigin);
        this._address = address;
    }

    withOffset(extraOffset)
    {
        return new AbsoluteAddress(codeOrigin, new Immediate(codeOrigin, this.address.value + extraOffset));
    }

    dump()
    {
        return this.address.dump() + "[]";
    }

    get address()
    {
        return this._address;
    }

    get isAddress()
    {
        return true;
    }

    get isLabel()
    {
        return false;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return true;
    }

    get isRegister()
    {
        return false;
    }
}

class Instruction extends Node
{
    constructor(codeOrigin, opcode, operands, annotation=nil)
    {
        super(codeOrigin);
        this._opcode = opcode;
        this._operands = operands;
        this._annotation = annotation;
    }

    children()
    {
        return [];
    }

    dump()
    {
        return "    " + this.opcode + " " + (this.operands.map(function(v) { return v.dump(); }).join(", "));
    }

    get opcode()
    {
        return this._opcode;
    }

    get operands()
    {
        return this._operands;
    }

    get annotation()
    {
        return this._annotation;
    }
}

class Error extends NoChildren
{
    constructor(codeOrigin)
    {
        super(codeOrigin);
    }

    dump()
    {
        return "    error";
    }
}

class ConstExpr extends  NoChildren
{
    constructor(codeOrigin, value)
    {
        super(codeOrigin);
        this._value = value;
    }

    static forName(codeOrigin, text)
    {
        if (!this.mapping[text])
            this.mapping[text] = new ConstExpr(codeOrigin, text);

        return this.mapping[text];
    }

    static resetMappings()
    {
        this.mapping = {};
    }

    dump()
    {
        return "constexpr (" + this.value + ")";
    }

    compare(other)
    {
        return this.value(other.value);
    }

    isImmediate()
    {
        return true;
    }

    get variable()
    {
        return this._variable;
    }

    get value()
    {
        return this._value;
    }
}

ConstExpr.mapping = {};


class ConstDecl extends Node
{
    constructor(codeOrigin, variable, value)
    {
        super(codeOrigin);
        this._variable = variable;
        this._value = value;
    }

    children()
    {
        return [this.variable, this.value];
    }

    dump()
    {
        return "const " + this.variable.dump() + " = " + this.value.dump();
    }

    get variable()
    {
        return this._variable;
    }

    get value()
    {
        return this._value;
    }
}

let _labelMapping = {};
let _referencedExternLabels = [];

class Label extends NoChildren
{
    constructor(codeOrigin, name)
    {
        super(codeOrigin);
        this._name = name;
        this._extern = true;
        this._global = false;
    }

    static forName(codeOrigin, name, definedInFile)
    {
        if (_labelMapping[name]) {
            if (!_labelMapping[name] instanceof Label)
                throw "Label name collision: " + name;
        } else
            _labelMapping[name] = new Label(codeOrigin, name);

        if (definedInFile)
            _labelMapping[name].clearExtern();

        return _labelMapping[name];
    }

    static setAsGlobal(codeOrigin, name)
    {
        if (_labelMapping[name]) {
            let label = _labelMapping[name];
            if (label.isGlobal())
                throw "Label: " + name + " declared global multiple times";
            label.setGlobal();
        } else {
            let newLabel = new Label(codeOrigin, name);
            newLabel.setGlobal();
            _labelMapping[name] = newLabel;
        }
    }

    static resetMappings()
    {
        _labelMapping = {};
        _referencedExternLabels = [];
    }


    static resetReferenced()
    {
        _referencedExternLabels = [];
    }

    clearExtern()
    {
        this._extern = false;
    }

    isExtern()
    {
        return this._extern;
    }

    setGlobal()
    {
        this._global = true;
    }

    isGlobal()
    {
        return this._global;
    }

    dump()
    {
        return this.name + ":";
    }

    get name()
    {
        return this._name;
    }
}

class LocalLabel extends NoChildren
{
    constructor(codeOrigin, name)
    {
        super(codeOrigin);
        this._name = name;
    }

    static forName(codeOrigin, name)
    {
        if (_labelMapping[name]) {
            if (!_labelMapping[name] instanceof LocalLabel)
                throw "Label name collision: " + name;
        } else
            _labelMapping[name] = new LocalLabel(codeOrigin, name);

        return _labelMapping[name];
    }

    static unique(comment)
    {
        let newName = "_" + comment;
        if (_labelMapping[newName]) {
            while (_labelMapping[newName = "_#" + this.uniqueNameCounter + "_" + comment])
                this.uniqueNameCounter++;
        }

        return forName(undefined, newName);
    }

    static resetMappings()
    {
        this.uniquNameCounter = 0;
    }

    cleanName()
    {
        if (/^\./.test(this._name))
            return "_" + this._name.slice(1);

        return this._name;
    }

    isGlobal()
    {
        return false;
    }

    dump()
    {
        return this.name + ":";
    }

    get name()
    {
        return this._name;
    }
}

LocalLabel.uniqueNameCounter = 0;


class LabelReference extends Node
{
    constructor(codeOrigin, label)
    {
        super(codeOrigin);
        this._label = label;
    }

    children()
    {
        return [this.label];
    }

    name()
    {
        return this.label.name;
    }

    isExtern()
    {
        return _labelMapping[name] instanceof Label && _labelMapping[name].isExtern();
    }

    used()
    {
        if (!_referencedExternLabels.include(this._label) && this.isExtern())
            _referencedExternLabels.push(this._label);
    }

    dump()
    {
        return this.label.name;
    }

    value()
    {
        return asmLabel();
    }

    get label()
    {
        return this._label;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return true;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return true;
    }
}

class LocalLabelReference extends NoChildren
{
    constructor(codeOrigin, label)
    {
        super(codeOrigin);
        this._label = label;
    }

    children()
    {
        return [this._label];
    }

    name()
    {
        return this.label.name;
    }

    dump()
    {
        return this.label.name;
    }

    value()
    {
        return asmLabel();
    }

    get label()
    {
        return this._label;
    }

    get isAddress()
    {
        return false;
    }

    get isLabel()
    {
        return true;
    }

    get isImmediate()
    {
        return false;
    }

    get isImmediateOperand()
    {
        return true;
    }
}

class Sequence extends Node
{
    constructor(codeOrigin, list)
    {
        super(codeOrigin);
        this._list = list;
    }

    children()
    {
        return this.list;
    }

    dump()
    {
        return "" + this.list.map(function(v) { return v.dump()}).join("\n");
    }

    get list()
    {
        return this._list;
    }
}

class True extends NoChildren
{
    constructor()
    {
        super(undefined);
    }

    static instance()
    {
        return True.instance;
    }

    value()
    {
        return true;
    }

    dump()
    {
        return "true";
    }
}

True.instance = new True();

class False extends NoChildren
{
    constructor()
    {
        super(undefined);
    }

    static instance()
    {
        return False.instance;
    }

    value()
    {
        return false;
    }

    dump()
    {
        return "false";
    }
}

False.instance = new False();

class Setting extends NoChildren
{
    constructor(codeOrigin, name)
    {
        super(codeOrigin);
        this._name = name;
    }

    static forName(codeOrigin, name)
    {
        if (!this.mapping[name])
            this.mapping[name] = new Setting(codeOrigin, name);

        return this.mapping[name];
    }

    static resetMappings()
    {
        this.mapping = {};
    }

    dump()
    {
        return this.name;
    }

    get name()
    {
        return this._name;
    }
}

Setting.mapping = {};

class And extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " and " + this.right.dump() + ")";
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }
}

class Or extends Node
{
    constructor(codeOrigin, left, right)
    {
        super(codeOrigin);
        this._left = left;
        this._right = right;
    }

    children()
    {
        return [this.left, this.right];
    }

    dump()
    {
        return "(" + this.left.dump() + " or " + this.right.dump() + ")";
    }

    get left()
    {
        return this._left;
    }

    get right()
    {
        return this._right;
    }
}

class Not extends Node
{
    constructor(codeOrigin, child)
    {
        super(codeOrigin);
        this._child = child;
    }

    children()
    {
        return [this.child];
    }

    dump()
    {
        return "(not" + this.child.dump() + ")";
    }

    get child()
    {
        return this._child;
    }
}

class Skip extends NoChildren
{
    constructor(codeOrigin)
    {
        super(codeOrigin);
    }

    dump()
    {
        return "    skip";
    }
}

class IfThenElse extends Node
{
    constructor(codeOrigin, predicate, thenCase)
    {
        super(codeOrigin);
        this._predicate = predicate;
        this._thenCase = thenCase;
        this._elseCase = new Skip(codeOrigin);
    }

    children()
    {
        return [];
    }

    dump()
    {
        return "if " + this.predicate.dump() + "\n" + this.thenCase.dump() + "\nelse\n" + this.elseCase.dump() + "\nend";
    }

    get predicate()
    {
        return this._predicate;
    }

    get thenCase()
    {
        return this._thenCase;
    }

    get elseCase()
    {
        return this._elseCase;
    }

    set elseCase(newElseCase)
    {
        this._elseCase = newElseCase;
    }
}

class Macro extends Node
{
    constructor(codeOrigin, name, variables, body)
    {
        super(codeOrigin);
        this._name = name;
        this._variables = variables;
        this._body = body;
    }

    children()
    {
        return [];
    }

    dump()
    {
        return "macro " + this.name + "(" + this.variables.map(function(v) { return v.dump()}).join(", ") + ")\n" + this.body.dump() + "\nend";
    }

    get name()
    {
        return this._name;
    }

    get variables()
    {
        return this._variables;
    }

    get body()
    {
        return this._body;
    }
}

class MacroCall extends Node
{
    constructor(codeOrigin, name, operands, annotation)
    {
        super(codeOrigin);
        this._name = name;
        this._operands = operands;
        if (!this._operands)
            throw "Operands empty to Macro call " + name;
        this._annotation = annotation;
    }

    children()
    {
        return [];
    }

    dump()
    {
        return "    " + this.name + "(" + this.operands.map(function(v) { return v.dump() }).join(", ") + ")";
    }

    get name()
    {
        return this._name;
    }

    get operands()
    {
        return this._operands;
    }

    get annotation()
    {
        return this._annotation;
    }
}

function resetAST()
{
    StructOffset.resetMappings();
    Sizeof.resetMappings();
    ConstExpr.resetMappings();
    Label.resetMappings();
    LocalLabel.resetMappings();
    Setting.resetMappings();
}
