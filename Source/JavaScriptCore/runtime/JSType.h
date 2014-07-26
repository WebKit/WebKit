/*
 *  Copyright (C) 2006, 2007, 2008, 2009, 2010, 2011 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#ifndef JSType_h
#define JSType_h

namespace JSC {

enum JSType : uint8_t {
    UnspecifiedType,
    UndefinedType,
    BooleanType,
    NumberType,
    NullType,
    StringType,
    LeafType,

    // The CompoundType value must come before any JSType that may have children.
    CompoundType,
    GetterSetterType,
    CustomGetterSetterType,
    APIValueWrapperType,

    EvalExecutableType,
    ProgramExecutableType,
    FunctionExecutableType,

    UnlinkedFunctionExecutableType,
    UnlinkedProgramCodeBlockType,
    UnlinkedEvalCodeBlockType,
    UnlinkedFunctionCodeBlockType,

    // The ObjectType value must come before any JSType that is a subclass of JSObject.
    ObjectType,
    FinalObjectType,
    JSFunctionType,
    NameInstanceType,
    NumberObjectType,
    ErrorInstanceType,
    PureForwardingProxyType,
    ImpureProxyType,
    WithScopeType,
    ArgumentsType,

    Int8ArrayType,
    Int16ArrayType,
    Int32ArrayType,
    Uint8ArrayType,
    Uint8ClampedArrayType,
    Uint16ArrayType,
    Uint32ArrayType,
    Float32ArrayType,
    Float64ArrayType,
    DataViewType,

    NameScopeObjectType,

    GlobalObjectType,
    ActivationObjectType,

    LastJSCObjectType = ActivationObjectType,
};

COMPILE_ASSERT(sizeof(JSType) == sizeof(uint8_t), sizeof_jstype_is_one_byte);

} // namespace JSC

#endif
