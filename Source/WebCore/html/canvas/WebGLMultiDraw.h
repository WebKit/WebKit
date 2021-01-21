/*
 * Copyright (C) 2021 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include "WebGLExtension.h"

namespace WebCore {

class WebGLMultiDraw final : public WebGLExtension {
    WTF_MAKE_ISO_ALLOCATED(WebGLMultiDraw);

private:
    template <class TypedArray, class DataType>
    class TypedList {
    public:
        using ListTypeOptions = Variant<RefPtr<TypedArray>, Vector<DataType>>;

        TypedList(ListTypeOptions&& variant)
            : m_variant(WTFMove(variant))
        {
        }

        const DataType* data() const
        {
            return WTF::switchOn(m_variant,
                [] (const RefPtr<TypedArray>& typedArray) -> const DataType* { return typedArray->data(); },
                [] (const Vector<DataType>& vector) -> const DataType* { return vector.data(); }
            );
        }

        GCGLsizei length() const
        {
            return WTF::switchOn(m_variant,
                [] (const RefPtr<TypedArray>& typedArray) -> GCGLsizei { return typedArray->length(); },
                [] (const Vector<DataType>& vector) -> GCGLsizei { return vector.size(); }
            );
        }

    private:
        ListTypeOptions m_variant;
    };

public:
    using Int32List = TypedList<Int32Array, int32_t>;

    explicit WebGLMultiDraw(WebGLRenderingContextBase&);
    virtual ~WebGLMultiDraw();

    ExtensionName getName() const override;

    static bool supported(const WebGLRenderingContextBase&);

    void multiDrawArraysWEBGL(GCGLenum mode, Int32List firstsList, GCGLuint firstsOffset, Int32List countsList, GCGLuint countsOffset, GCGLsizei drawcount);

    void multiDrawElementsWEBGL(GCGLenum mode, Int32List countsList, GCGLuint countsOffset, GCGLenum type, Int32List offsetsList, GCGLuint offsetsOffset, GCGLsizei drawcount);

    void multiDrawArraysInstancedWEBGL(GCGLenum mode, Int32List firstsList, GCGLuint firstsOffset, Int32List countsList, GCGLuint countsOffset, Int32List instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount);

    void multiDrawElementsInstancedWEBGL(GCGLenum mode, Int32List countsList, GCGLuint countsOffset, GCGLenum type, Int32List offsetsList, GCGLuint offsetsOffset, Int32List instanceCountsList, GCGLuint instanceCountsOffset, GCGLsizei drawcount);

private:
    bool validateDrawcount(const char* functionName, GCGLsizei drawcount);
    bool validateOffset(const char* functionName, const char* outOfBoundsDescription, GCGLsizei, GCGLuint offset, GCGLsizei drawcount);
};

} // namespace WebCore
