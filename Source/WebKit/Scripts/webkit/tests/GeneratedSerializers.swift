//
// Copyright (C) 2022-2023 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1.  Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
// 2.  Redistributions in binary form must reproduce the above copyright
// notice, this list of conditions and the following disclaimer in the
// documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND
// ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
// WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
// DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR
// ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
// DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
// SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
// CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
// OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
//

#if ENABLE_BACK_FORWARD_LIST_SWIFT || (ENABLE_IPC_TESTING_API && ENABLE_IPC_TESTING_SWIFT)

import WebKit_Internal

extension IPCTypes {
    enum FrameIdentifierType {}
    typealias FrameIdentifier = IPCObjectIdentifier<FrameIdentifierType>
    enum SwiftAtomicTestIdentifierType {}
    typealias SwiftAtomicTestIdentifier = IPCObjectIdentifier<SwiftAtomicTestIdentifierType>
}

extension IPCTypes {
    enum SwiftEnum: UInt8, Sendable, Hashable {
        case firstValue = 0
        case httpsOnly = 1
        case url = 2
        case `default` = 3
        #if ENABLE_SWIFT_ENUM_VALUE
        case conditionalValue = 4
        #endif
    }
}

extension IPCTypes.SwiftEnum: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftEnum {
        guard let value = IPCTypes.SwiftEnum(rawValue: try UInt8.decode(from: &decoder)) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: IPCTypes.SwiftEnum, to encoder: IPC.SwiftReplyEncoder) {
        UInt8.encode(value.rawValue, to: encoder)
    }
}

extension IPCTypes {
    enum SwiftBoolEnum: UInt8, Sendable, Hashable {
        case no = 0
        case yes = 1
    }
}

extension IPCTypes.SwiftBoolEnum: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftBoolEnum {
        guard let value = IPCTypes.SwiftBoolEnum(rawValue: try Bool.decode(from: &decoder) ? 1 : 0) else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: IPCTypes.SwiftBoolEnum, to encoder: IPC.SwiftReplyEncoder) {
        Bool.encode(value.rawValue != 0, to: encoder)
    }
}

extension IPCTypes {
    struct SwiftOptionSet: OptionSet, Sendable, Hashable {
        let rawValue: UInt16

        static let firstOption = SwiftOptionSet(rawValue: 1 << 0)
        #if ENABLE_SWIFT_OPTION
        static let conditionalOption = SwiftOptionSet(rawValue: 1 << 1)
        #endif
        static let thirdOption = SwiftOptionSet(rawValue: 1 << 2)
    }
}

extension IPCTypes.SwiftOptionSet: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftOptionSet {
        let value = IPCTypes.SwiftOptionSet(rawValue: try UInt16.decode(from: &decoder))
        var unknownValues = value
        unknownValues.subtract(.firstOption)
        #if ENABLE_SWIFT_OPTION
        unknownValues.subtract(.conditionalOption)
        #endif
        unknownValues.subtract(.thirdOption)
        guard unknownValues.isEmpty else {
            throw IPCDecodingError()
        }
        return value
    }

    static func encode(_ value: IPCTypes.SwiftOptionSet, to encoder: IPC.SwiftReplyEncoder) {
        UInt16.encode(value.rawValue, to: encoder)
    }
}

extension IPCTypes {
    struct SwiftInner: Sendable, Equatable {
        var value: Int32
        var label: String?
    }
}

extension IPCTypes.SwiftInner: IPCCoder {
    fileprivate init(ipcDecodingFrom decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) {
        self.value = try Int32.decode(from: &decoder)
        self.label = try IPCCoding.WTFString.decode(from: &decoder)
    }

    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftInner {
        try IPCTypes.SwiftInner(ipcDecodingFrom: &decoder)
    }

    static func encode(_ value: IPCTypes.SwiftInner, to encoder: IPC.SwiftReplyEncoder) {
        Int32.encode(value.value, to: encoder)
        IPCCoding.WTFString.encode(value.label, to: encoder)
    }
}

extension IPCTypes {
    struct SwiftStruct: Sendable, Equatable {
        var flag: Bool
        var name: String?
        var numbers: [UInt64]
        var strings: [String?]
        var optionalDouble: Double?
        var markableFrameID: IPCTypes.FrameIdentifier?
        var frameID: IPCTypes.FrameIdentifier
        var atomicID: IPCTypes.SwiftAtomicTestIdentifier
        var inner: IPCTypes.SwiftInner
        var optionalInner: IPCTypes.SwiftInner?
        var enumValue: IPCTypes.SwiftEnum
        var boolEnum: IPCTypes.SwiftBoolEnum
        var options: IPCTypes.SwiftOptionSet
        var variant: IPCTypes.SwiftVariant
        var `default`: Bool
        #if ENABLE_SWIFT_MEMBER
        var conditionalMember: UInt32
        #endif
        var accessor: Float
    }
}

extension IPCTypes.SwiftStruct: IPCCoder {
    fileprivate init(ipcDecodingFrom decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) {
        self.flag = try Bool.decode(from: &decoder)
        self.name = try IPCCoding.WTFString.decode(from: &decoder)
        self.numbers = try IPCCoding.Vector<UInt64>.decode(from: &decoder)
        self.strings = try IPCCoding.Vector<IPCCoding.WTFString>.decode(from: &decoder)
        self.optionalDouble = try IPCCoding.StdOptional<Double>.decode(from: &decoder)
        self.markableFrameID = try IPCCoding.Markable<IPCTypes.FrameIdentifier>.decode(from: &decoder)
        self.frameID = try IPCTypes.FrameIdentifier.decode(from: &decoder)
        self.atomicID = try IPCTypes.SwiftAtomicTestIdentifier.decode(from: &decoder)
        self.inner = try IPCTypes.SwiftInner.decode(from: &decoder)
        self.optionalInner = try IPCCoding.StdOptional<IPCTypes.SwiftInner>.decode(from: &decoder)
        self.enumValue = try IPCTypes.SwiftEnum.decode(from: &decoder)
        self.boolEnum = try IPCTypes.SwiftBoolEnum.decode(from: &decoder)
        self.options = try IPCTypes.SwiftOptionSet.decode(from: &decoder)
        self.variant = try IPCTypes.SwiftVariant.decode(from: &decoder)
        self.`default` = try Bool.decode(from: &decoder)
        #if ENABLE_SWIFT_MEMBER
        self.conditionalMember = try UInt32.decode(from: &decoder)
        #endif
        self.accessor = try Float.decode(from: &decoder)
    }

    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftStruct {
        try IPCTypes.SwiftStruct(ipcDecodingFrom: &decoder)
    }

    static func encode(_ value: IPCTypes.SwiftStruct, to encoder: IPC.SwiftReplyEncoder) {
        Bool.encode(value.flag, to: encoder)
        IPCCoding.WTFString.encode(value.name, to: encoder)
        IPCCoding.Vector<UInt64>.encode(value.numbers, to: encoder)
        IPCCoding.Vector<IPCCoding.WTFString>.encode(value.strings, to: encoder)
        IPCCoding.StdOptional<Double>.encode(value.optionalDouble, to: encoder)
        IPCCoding.Markable<IPCTypes.FrameIdentifier>.encode(value.markableFrameID, to: encoder)
        IPCTypes.FrameIdentifier.encode(value.frameID, to: encoder)
        IPCTypes.SwiftAtomicTestIdentifier.encode(value.atomicID, to: encoder)
        IPCTypes.SwiftInner.encode(value.inner, to: encoder)
        IPCCoding.StdOptional<IPCTypes.SwiftInner>.encode(value.optionalInner, to: encoder)
        IPCTypes.SwiftEnum.encode(value.enumValue, to: encoder)
        IPCTypes.SwiftBoolEnum.encode(value.boolEnum, to: encoder)
        IPCTypes.SwiftOptionSet.encode(value.options, to: encoder)
        IPCTypes.SwiftVariant.encode(value.variant, to: encoder)
        Bool.encode(value.`default`, to: encoder)
        #if ENABLE_SWIFT_MEMBER
        UInt32.encode(value.conditionalMember, to: encoder)
        #endif
        Float.encode(value.accessor, to: encoder)
    }
}

extension IPCTypes {
    final class SwiftNode: Sendable, Equatable {
        let name: String?
        let children: [IPCTypes.SwiftNode]
        let parent: IPCTypes.SwiftNode?
        let leaf: IPCTypes.SwiftLeaf

        init(
            name: String?,
            children: [IPCTypes.SwiftNode],
            parent: IPCTypes.SwiftNode?,
            leaf: IPCTypes.SwiftLeaf
        ) {
            self.name = name
            self.children = children
            self.parent = parent
            self.leaf = leaf
        }

        fileprivate init(ipcDecodingFrom decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) {
            self.name = try IPCCoding.WTFString.decode(from: &decoder)
            self.children = try IPCCoding.Vector<IPCTypes.SwiftNode>.decode(from: &decoder)
            self.parent = try IPCCoding.StdOptional<IPCTypes.SwiftNode>.decode(from: &decoder)
            self.leaf = try IPCTypes.SwiftLeaf.decode(from: &decoder)
        }

        static func == (lhs: IPCTypes.SwiftNode, rhs: IPCTypes.SwiftNode) -> Bool {
            if lhs === rhs {
                return true
            }
            guard lhs.name == rhs.name else {
                return false
            }
            guard lhs.children == rhs.children else {
                return false
            }
            guard lhs.parent == rhs.parent else {
                return false
            }
            guard lhs.leaf == rhs.leaf else {
                return false
            }
            return true
        }
    }
}

extension IPCTypes.SwiftNode: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftNode {
        try IPCTypes.SwiftNode(ipcDecodingFrom: &decoder)
    }

    static func encode(_ value: IPCTypes.SwiftNode, to encoder: IPC.SwiftReplyEncoder) {
        IPCCoding.WTFString.encode(value.name, to: encoder)
        IPCCoding.Vector<IPCTypes.SwiftNode>.encode(value.children, to: encoder)
        IPCCoding.StdOptional<IPCTypes.SwiftNode>.encode(value.parent, to: encoder)
        IPCTypes.SwiftLeaf.encode(value.leaf, to: encoder)
    }
}

extension IPCTypes {
    final class SwiftLeaf: Sendable, Equatable {
        let value: UInt64
        #if ENABLE_SWIFT_MEMBER
        let conditionalValue: Double
        #endif

        fileprivate init(ipcDecodingFrom decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) {
            self.value = try UInt64.decode(from: &decoder)
            #if ENABLE_SWIFT_MEMBER
            self.conditionalValue = try Double.decode(from: &decoder)
            #endif
        }

        static func == (lhs: IPCTypes.SwiftLeaf, rhs: IPCTypes.SwiftLeaf) -> Bool {
            if lhs === rhs {
                return true
            }
            guard lhs.value == rhs.value else {
                return false
            }
            #if ENABLE_SWIFT_MEMBER
            guard lhs.conditionalValue == rhs.conditionalValue else {
                return false
            }
            #endif
            return true
        }
    }
}

extension IPCTypes.SwiftLeaf: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftLeaf {
        try IPCTypes.SwiftLeaf(ipcDecodingFrom: &decoder)
    }

    static func encode(_ value: IPCTypes.SwiftLeaf, to encoder: IPC.SwiftReplyEncoder) {
        UInt64.encode(value.value, to: encoder)
        #if ENABLE_SWIFT_MEMBER
        Double.encode(value.conditionalValue, to: encoder)
        #endif
    }
}

#if ENABLE_SWIFT_TYPE_CONDITION
extension IPCTypes {
    struct SwiftDerived: Sendable, Equatable {
        var baseValue: UInt32
        var derivedValue: Double
    }
}

extension IPCTypes.SwiftDerived: IPCCoder {
    fileprivate init(ipcDecodingFrom decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) {
        self.baseValue = try UInt32.decode(from: &decoder)
        self.derivedValue = try Double.decode(from: &decoder)
    }

    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftDerived {
        try IPCTypes.SwiftDerived(ipcDecodingFrom: &decoder)
    }

    static func encode(_ value: IPCTypes.SwiftDerived, to encoder: IPC.SwiftReplyEncoder) {
        UInt32.encode(value.baseValue, to: encoder)
        Double.encode(value.derivedValue, to: encoder)
    }
}
#endif

extension IPCTypes {
    enum SwiftVariant: Sendable, Equatable {
        case empty
        case uint64(UInt64)
        case string(String?)
        case vectorString([String?])
        case swiftInner(IPCTypes.SwiftInner)
        case swiftNode(IPCTypes.SwiftNode)
    }
}

extension IPCTypes.SwiftVariant: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftVariant {
        switch try UInt8.decode(from: &decoder) {
        case 0:
            return .empty
        case 1:
            return .uint64(try UInt64.decode(from: &decoder))
        case 2:
            return .string(try IPCCoding.WTFString.decode(from: &decoder))
        case 3:
            return .vectorString(try IPCCoding.Vector<IPCCoding.WTFString>.decode(from: &decoder))
        case 4:
            return .swiftInner(try IPCTypes.SwiftInner.decode(from: &decoder))
        case 5:
            return .swiftNode(try IPCTypes.SwiftNode.decode(from: &decoder))
        default:
            throw IPCDecodingError()
        }
    }

    static func encode(_ value: IPCTypes.SwiftVariant, to encoder: IPC.SwiftReplyEncoder) {
        switch value {
        case .empty:
            UInt8.encode(0, to: encoder)
        case .uint64(let alternative):
            UInt8.encode(1, to: encoder)
            UInt64.encode(alternative, to: encoder)
        case .string(let alternative):
            UInt8.encode(2, to: encoder)
            IPCCoding.WTFString.encode(alternative, to: encoder)
        case .vectorString(let alternative):
            UInt8.encode(3, to: encoder)
            IPCCoding.Vector<IPCCoding.WTFString>.encode(alternative, to: encoder)
        case .swiftInner(let alternative):
            UInt8.encode(4, to: encoder)
            IPCTypes.SwiftInner.encode(alternative, to: encoder)
        case .swiftNode(let alternative):
            UInt8.encode(5, to: encoder)
            IPCTypes.SwiftNode.encode(alternative, to: encoder)
        }
    }
}

extension IPCTypes {
    enum SwiftMultiLineVariant: Sendable, Equatable {
        case frameIdentifier(IPCTypes.FrameIdentifier)
        case optionalSwiftEnum(IPCTypes.SwiftEnum?)
    }
}

extension IPCTypes.SwiftMultiLineVariant: IPCCoder {
    static func decode(from decoder: inout IPC.SwiftDecoder) throws(IPCDecodingError) -> IPCTypes.SwiftMultiLineVariant {
        switch try UInt8.decode(from: &decoder) {
        case 0:
            return .frameIdentifier(try IPCTypes.FrameIdentifier.decode(from: &decoder))
        case 1:
            return .optionalSwiftEnum(try IPCCoding.StdOptional<IPCTypes.SwiftEnum>.decode(from: &decoder))
        default:
            throw IPCDecodingError()
        }
    }

    static func encode(_ value: IPCTypes.SwiftMultiLineVariant, to encoder: IPC.SwiftReplyEncoder) {
        switch value {
        case .frameIdentifier(let alternative):
            UInt8.encode(0, to: encoder)
            IPCTypes.FrameIdentifier.encode(alternative, to: encoder)
        case .optionalSwiftEnum(let alternative):
            UInt8.encode(1, to: encoder)
            IPCCoding.StdOptional<IPCTypes.SwiftEnum>.encode(alternative, to: encoder)
        }
    }
}

extension IPCTypes {
    /// The Swift forms of C++ types by qualified C++ name, so Swift naming a C++ type that has none does not compile.
    enum CxxName {
        typealias Namespace_SwiftBoolEnum = IPCTypes.SwiftBoolEnum
        #if ENABLE_SWIFT_TYPE_CONDITION
        typealias Namespace_SwiftDerived = IPCTypes.SwiftDerived
        #endif
        typealias Namespace_SwiftEnum = IPCTypes.SwiftEnum
        typealias Namespace_SwiftInner = IPCTypes.SwiftInner
        typealias Namespace_SwiftLeaf = IPCTypes.SwiftLeaf
        typealias Namespace_SwiftMultiLineVariant = IPCTypes.SwiftMultiLineVariant
        typealias Namespace_SwiftNode = IPCTypes.SwiftNode
        typealias Namespace_SwiftOptionSet = IPCTypes.SwiftOptionSet
        typealias Namespace_SwiftStruct = IPCTypes.SwiftStruct
        typealias Namespace_SwiftVariant = IPCTypes.SwiftVariant
        typealias WebCore_FrameIdentifier = IPCTypes.FrameIdentifier
        typealias WebKit_SwiftAtomicTestIdentifier = IPCTypes.SwiftAtomicTestIdentifier
    }
}

#endif
