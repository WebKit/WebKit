// Copyright (C) 2026 Apple Inc. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions
// are met:
// 1. Redistributions of source code must retain the above copyright
//    notice, this list of conditions and the following disclaimer.
// 2. Redistributions in binary form must reproduce the above copyright
//    notice, this list of conditions and the following disclaimer in the
//    documentation and/or other materials provided with the distribution.
//
// THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
// AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
// THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
// PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
// BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
// CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
// SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
// CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
// ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
// THE POSSIBILITY OF SUCH DAMAGE.

import SwiftSyntax
import SwiftSyntaxMacros

enum CxxCopyMacroError: Error, CustomStringConvertible {
    case notAProperty
    case missingTypeAnnotation
    case hasOwnAccessors

    var description: String {
        switch self {
        case .notAProperty:
            "@CxxCopy must be attached to a computed property declaration"
        case .missingTypeAnnotation:
            "@CxxCopy requires an explicit type annotation naming the C++ type to copy"
        case .hasOwnAccessors:
            "@CxxCopy must not be combined with a hand-written accessor"
        }
    }
}

/// Implements `@CxxCopy`, which supplies the getter for a property that shadows
/// a C++ getter returning `const T&`.
///
/// The clang importer renames such a getter to `__<name>Unsafe()`, returning an
/// `UnsafePointer<T>`. This macro generates the dereference, so exactly one
/// `unsafe` appears per accessor, inside generated code, instead of at every
/// call site.
public struct CxxCopyAccessorMacro: AccessorMacro {
    public static func expansion(
        of node: AttributeSyntax,
        providingAccessorsOf declaration: some DeclSyntaxProtocol,
        in context: some MacroExpansionContext
    ) throws -> [AccessorDeclSyntax] {
        guard let variable = declaration.as(VariableDeclSyntax.self),
            let binding = variable.bindings.first,
            let identifier = binding.pattern.as(IdentifierPatternSyntax.self)
        else {
            throw CxxCopyMacroError.notAProperty
        }
        guard binding.typeAnnotation != nil else {
            throw CxxCopyMacroError.missingTypeAnnotation
        }
        guard binding.accessorBlock == nil else {
            throw CxxCopyMacroError.hasOwnAccessors
        }

        let importedName = "__\(identifier.identifier.text)Unsafe"
        return [
            """
            get {
                // Safety: the pointee is copied before returning, so nothing
                // outlives the C++ getter's LIFETIME_BOUND result.
                unsafe self.\(raw: importedName)().pointee
            }
            """
        ]
    }
}
