/*
 *  Copyright (C) 2024 Igalia S.L.
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

#pragma once

#include <glib.h>
#include <optional>

#include <wtf/glib/GUniquePtr.h>
#include <wtf/text/CString.h>

namespace IPC {

template<> struct ArgumentCoder<GUniquePtr<char*>> {
    static void encode(Encoder& encoder, const GUniquePtr<char*>& strv)
    {
        auto length = strv.get() ? g_strv_length(strv.get()) : 0;

        encoder << length;

        if (!length)
            return;

        for (uint32_t i = 0; strv.get()[i]; i++)
            encoder << CString(strv.get()[i]);
    }

    static std::optional<GUniquePtr<char*>> decode(Decoder& decoder)
    {
        auto length = decoder.decode<unsigned>();

        if (UNLIKELY(!length))
            return std::nullopt;

        GUniquePtr<char*>strv(g_new0(char*, *length + 1));
        for (uint32_t i = 0; i < *length; i++) {
            auto strOptional = decoder.decode<CString>();
            if (UNLIKELY(!strOptional))
                return std::nullopt;

            strv.get()[i] = g_strdup(strOptional->data());
        }

        return strv;
    }
};

template<typename T> struct ArgumentCoder<GRefPtr<T>> {
    template<typename Encoder, typename U = T>
    static void encode(Encoder& encoder, const GRefPtr<U>& object)
    {
        if (object)
            encoder << true << object.get();
        else
            encoder << false;
    }

    template<typename Decoder, typename U = T>
    static std::optional<GRefPtr<U>> decode(Decoder& decoder)
    {
        auto hasObject = decoder.template decode<bool>();
        if (!hasObject)
            return std::nullopt;
        if (!*hasObject)
            return GRefPtr<U> { };
        auto object = decoder.template decode<U *>();
        if (!object)
            return std::nullopt;
        return adoptGRef(*object);
    }
};

} // namespace IPC
