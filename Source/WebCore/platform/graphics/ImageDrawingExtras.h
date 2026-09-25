/*
 * Copyright (C) 2026 Samuel Weinig <sam@webkit.org>
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
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#pragma once

#include <WebCore/PlatformExportMacros.h>
#include <cstdint>
#include <memory>
#include <wtf/TZoneMalloc.h>

namespace WebCore {

class ImageDrawingExtras {
    WTF_MAKE_TZONE_ALLOCATED_EXPORT(ImageDrawingExtras, WEBCORE_EXPORT);
public:
    enum class Type : uint8_t {
        Style,
    };

    virtual ~ImageDrawingExtras() = default;

    Type type() const { return m_type; }

    virtual std::unique_ptr<ImageDrawingExtras> copy() const = 0;
    bool operator==(const ImageDrawingExtras& other) const { return m_type == other.m_type && equals(other); }

protected:
    explicit ImageDrawingExtras(Type type)
        : m_type(type)
    {
    }
    ImageDrawingExtras(const ImageDrawingExtras&) = default;

    virtual bool equals(const ImageDrawingExtras&) const = 0;

private:
    Type m_type;
};

} // namespace WebCore
