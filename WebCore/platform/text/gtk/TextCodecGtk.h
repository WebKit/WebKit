/*
 * Copyright (C) 2004, 2006, 2007 Apple Inc. All rights reserved.
 * Copyright (C) 2006 Alexey Proskuryakov <ap@nypop.com>
 * Copyright (C) 2008 Jürg Billeter <j@bitron.ch>
 * Copyright (C) 2009 Dominik Röttsches <dominik.roettsches@access-company.com>
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#ifndef TextCodecGTK_h
#define TextCodecGTK_h

#include <glib.h>
#include "TextCodec.h"
#include "TextEncoding.h"

namespace WebCore {

    class TextCodecGtk : public TextCodec {
    public:
        static void registerBaseEncodingNames(EncodingNameRegistrar);
        static void registerBaseCodecs(TextCodecRegistrar);

        static void registerExtendedEncodingNames(EncodingNameRegistrar);
        static void registerExtendedCodecs(TextCodecRegistrar);

        TextCodecGtk(const TextEncoding&);
        virtual ~TextCodecGtk();

        virtual String decode(const char*, size_t length, bool flush, bool stopOnError, bool& sawError);
        virtual CString encode(const UChar*, size_t length, UnencodableHandling);

    private:
        void createIConvDecoder() const;
        void createIConvEncoder() const;

        static void registerEncodingNames(EncodingNameRegistrar registrar, bool extended);        
        static void registerCodecs(TextCodecRegistrar registrar, bool extended);
        static gboolean isEncodingAvailable(const gchar*);

        TextEncoding m_encoding;
        size_t m_numBufferedBytes;
        unsigned char m_bufferedBytes[16]; // bigger than any single multi-byte character        
        mutable GIConv m_iconvDecoder;
        mutable GIConv m_iconvEncoder;

        static const gchar* m_internalEncodingName;

        typedef const gchar* const codecAliasList[];

        // Unicode
        static codecAliasList m_codecAliases_UTF_8;

        // Western
        static codecAliasList m_codecAliases_ISO_8859_1;
        static codecAliasList m_codecAliases_MACROMAN;

        // Japanese
        static codecAliasList m_codecAliases_SHIFT_JIS;
        static codecAliasList m_codecAliases_EUC_JP;
        static codecAliasList m_codecAliases_ISO_2022_JP;

        // Traditional Chinese
        static codecAliasList m_codecAliases_BIG5;
        static codecAliasList m_codecAliases_BIG5_HKSCS;
        static codecAliasList m_codecAliases_CP950;

        // Korean
        static codecAliasList m_codecAliases_ISO_2022_KR;
        static codecAliasList m_codecAliases_CP949;
        static codecAliasList m_codecAliases_EUC_KR;

        // Arabic
        static codecAliasList m_codecAliases_ISO_8859_6;
        static codecAliasList m_codecAliases_CP1256;

        // Hebrew
        static codecAliasList m_codecAliases_ISO_8859_8;
        static codecAliasList m_codecAliases_CP1255;

        // Greek
        static codecAliasList m_codecAliases_ISO_8859_7;
        static codecAliasList m_codecAliases_CP869;
        static codecAliasList m_codecAliases_WINDOWS_1253;

        // Cyrillic
        static codecAliasList m_codecAliases_ISO_8859_5;
        static codecAliasList m_codecAliases_KOI8_R;
        static codecAliasList m_codecAliases_CP866;
        static codecAliasList m_codecAliases_KOI8_U;
        static codecAliasList m_codecAliases_WINDOWS_1251;
        static codecAliasList m_codecAliases_MACCYRILLIC;

        // Thai
        static codecAliasList m_codecAliases_CP874;
        static codecAliasList m_codecAliases_TIS_620;

        // Simplified Chinese
        static codecAliasList m_codecAliases_GBK;
        static codecAliasList m_codecAliases_HZ;
        static codecAliasList m_codecAliases_GB18030;
        static codecAliasList m_codecAliases_EUC_CN;
        static codecAliasList m_codecAliases_2312_80; 

        // Central European
        static codecAliasList m_codecAliases_ISO_8859_2;
        static codecAliasList m_codecAliases_CP1250;
        static codecAliasList m_codecAliases_MACCENTRALEUROPE;

        // Vietnamese
        static codecAliasList m_codecAliases_CP1258;

        // Turkish
        static codecAliasList m_codecAliases_CP1254;
        static codecAliasList m_codecAliases_ISO_8859_9;

        // Baltic
        static codecAliasList m_codecAliases_CP1257;
        static codecAliasList m_codecAliases_ISO_8859_4;

        static gconstpointer const m_iconvBaseCodecList[];
        static gconstpointer const m_iconvExtendedCodecList[];

    };

} // namespace WebCore

#endif // TextCodecGTK_h
