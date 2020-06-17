/*
 * Copyright (C) 2007-2020 Apple Inc. All rights reserved.
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

#include <limits.h>
#include <wtf/EnumTraits.h>
#include <wtf/Forward.h>
#include <wtf/OptionSet.h>

namespace WebCore {

// See WebDragDestinationAction and WKDragDestinationAction.
enum class DragDestinationAction : uint8_t {
    DHTML = 1,
    Edit  = 2,
    Load  = 4
};

constexpr OptionSet<DragDestinationAction> anyDragDestinationAction()
{
    return OptionSet<DragDestinationAction> { DragDestinationAction::DHTML, DragDestinationAction::Edit, DragDestinationAction::Load };
}

// See WebDragSourceAction.
enum class DragSourceAction : uint8_t {
    DHTML      = 1,
    Image      = 2,
    Link       = 4,
    Selection  = 8,
#if ENABLE(ATTACHMENT_ELEMENT)
    Attachment = 16,
#endif
#if ENABLE(INPUT_TYPE_COLOR)
    Color      = 32,
#endif
};

constexpr OptionSet<DragSourceAction> anyDragSourceAction()
{
    return OptionSet<DragSourceAction> {
        DragSourceAction::DHTML,
        DragSourceAction::Image,
        DragSourceAction::Link,
        DragSourceAction::Selection
#if ENABLE(ATTACHMENT_ELEMENT)
        , DragSourceAction::Attachment
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        , DragSourceAction::Color
#endif
    };
}

// See NSDragOperation, _UIDragOperation and UIDropOperation.
enum class DragOperation : uint8_t {
    Copy    = 1,
    Link    = 2,
    Generic = 4,
    Private = 8,
    Move    = 16,
    Delete  = 32,
};

constexpr OptionSet<DragOperation> anyDragOperation()
{
    return { DragOperation::Copy, DragOperation::Link, DragOperation::Generic, DragOperation::Private, DragOperation::Move, DragOperation::Delete };
}

enum class MayExtendDragSession : bool { No, Yes };
enum class HasNonDefaultPasteboardData : bool { No, Yes };
enum class DragHandlingMethod : uint8_t { None, EditPlainText, EditRichText, UploadFile, PageLoad, SetColor, NonDefault };

} // namespace WebCore

namespace WTF {

template<> struct EnumTraits<WebCore::DragDestinationAction> {
    using values = EnumValues<
        WebCore::DragDestinationAction,
        WebCore::DragDestinationAction::DHTML,
        WebCore::DragDestinationAction::Edit,
        WebCore::DragDestinationAction::Load
    >;
};

template<> struct EnumTraits<WebCore::DragHandlingMethod> {
    using values = EnumValues<
        WebCore::DragHandlingMethod,
        WebCore::DragHandlingMethod::None,
        WebCore::DragHandlingMethod::EditPlainText,
        WebCore::DragHandlingMethod::EditRichText,
        WebCore::DragHandlingMethod::UploadFile,
        WebCore::DragHandlingMethod::PageLoad,
        WebCore::DragHandlingMethod::SetColor,
        WebCore::DragHandlingMethod::NonDefault
    >;
};

template<> struct EnumTraits<WebCore::DragOperation> {
    using values = EnumValues<
        WebCore::DragOperation,
        WebCore::DragOperation::Copy,
        WebCore::DragOperation::Link,
        WebCore::DragOperation::Generic,
        WebCore::DragOperation::Private,
        WebCore::DragOperation::Move,
        WebCore::DragOperation::Delete
    >;
};

template<> struct EnumTraits<WebCore::DragSourceAction> {
    using values = EnumValues<
        WebCore::DragSourceAction,
        WebCore::DragSourceAction::DHTML,
        WebCore::DragSourceAction::Image,
        WebCore::DragSourceAction::Link,
        WebCore::DragSourceAction::Selection
#if ENABLE(ATTACHMENT_ELEMENT)
        , WebCore::DragSourceAction::Attachment
#endif
#if ENABLE(INPUT_TYPE_COLOR)
        , WebCore::DragSourceAction::Color
#endif
    >;
};

} // namespace WTF
