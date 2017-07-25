/*
 *  Copyright (C) 2006-2017 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 */

#pragma once

namespace WebCore {

// Some of these are considered historical since they have been
// changed or removed from the specifications.
enum ExceptionCode {
    NoException = 0,
    IndexSizeError = 1,
    HierarchyRequestError = 3,
    WrongDocumentError = 4,
    InvalidCharacterError = 5,
    NoModificationAllowedError = 7,
    NotFoundError = 8,
    NotSupportedError = 9,
    InUseAttributeError = 10, // Historical. Only used in setAttributeNode etc which have been removed from the DOM specs.

    // Introduced in DOM Level 2:
    InvalidStateError = 11,
    SyntaxError = 12,
    InvalidModificationError = 13,
    NamespaceError = 14,
    InvalidAccessError = 15,

    // Introduced in DOM Level 3:
    TypeMismatchError = 17, // Historical; use TypeError instead

    // XMLHttpRequest extension:
    SecurityError = 18,

    // Others introduced in HTML5:
    NetworkError = 19,
    AbortError = 20,
    URLMismatchError = 21,
    QuotaExceededError = 22,
    TimeoutError = 23,
    InvalidNodeTypeError = 24,
    DataCloneError = 25,

    // Others introduced in https://heycam.github.io/webidl/#idl-exceptions
    EncodingError,
    NotReadableError,
    UnknownError,
    ConstraintError,
    DataError,
    TransactionInactiveError,
    ReadonlyError,
    VersionError,
    OperationError,
    NotAllowedError,

    // Non-standard errors
    StackOverflowError,

    // Used to indicate to the bindings that a JS exception was thrown below and it should be propogated.
    ExistingExceptionError,

    // WebIDL exception types, handled by the binding layer.
    // FIXME: Add GeneralError, EvalError, etc. when implemented in the bindings.
    TypeError = 105,
    RangeError = 106,
};

} // namespace WebCore
