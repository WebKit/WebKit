/*
 * Copyright (C) 2018 Igalia S.L.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <wtf/text/WTFString.h>

namespace WebDriver {

enum class MouseButton { None, Left, Middle, Right };
enum class PointerType { Mouse, Pen, Touch };

struct InputSource {
    enum class Type { None, Key, Pointer };

    Type type;
    std::optional<PointerType> pointerType;
};

struct PointerParameters {
    PointerType pointerType { PointerType::Mouse };
};

struct PointerOrigin {
    enum class Type { Viewport, Pointer, Element };

    Type type;
    std::optional<String> elementID;
};

struct Action {
    enum class Type { None, Key, Pointer };
    enum class Subtype { Pause, PointerUp, PointerDown, PointerMove, PointerCancel, KeyUp, KeyDown };

    Action(const String& id, Type type, Subtype subtype)
        : id(id)
        , type(type)
        , subtype(subtype)
    {
    }

    String id;
    Type type;
    Subtype subtype;
    std::optional<unsigned> duration;

    std::optional<PointerType> pointerType;
    std::optional<MouseButton> button;
    std::optional<PointerOrigin> origin;
    std::optional<int64_t> x;
    std::optional<int64_t> y;

    std::optional<String> key;
};

} // WebDriver
