/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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

#import "KWQTextCodec.h"

#import "KWQAssertions.h"
#import "KWQCharsets.h"

struct TECObjectPeek {
    UInt32 skip1;
    UInt32 skip2;
    UInt32 skip3;
    OptionBits optionsControlFlags;
};

class KWQTextDecoder : public QTextDecoder {
public:
    KWQTextDecoder(CFStringEncoding, KWQEncodingFlags);
    ~KWQTextDecoder();
    
    QString toUnicode(const char *chs, int len, bool flush);

private:
    QString convertUTF16(const unsigned char *chs, int len);
    QString convertUsingTEC(const UInt8 *chs, int len, bool flush);
    
    KWQTextDecoder(const KWQTextDecoder &);
    KWQTextDecoder &operator=(const KWQTextDecoder &);

    CFStringEncoding _encoding;
    KWQEncodingFlags _flags;
    
    // State for Unicode decoding.
    enum UnicodeEndianState {
        atStart,
        littleEndian,
        bigEndian
    };
    UnicodeEndianState _state;
    bool _haveBufferedByte;
    char _bufferedByte;
    
    // State for TEC decoding.
    TECObjectRef _converter;
    static TECObjectRef _cachedConverter;
    static CFStringEncoding _cachedConverterEncoding;
};

TECObjectRef KWQTextDecoder::_cachedConverter;
CFStringEncoding KWQTextDecoder::_cachedConverterEncoding = kCFStringEncodingInvalidId;

static Boolean QTextCodecsEqual(const void *value1, const void *value2);
static CFHashCode QTextCodecHash(const void *value);

static QTextCodec *codecForCFStringEncoding(CFStringEncoding encoding, KWQEncodingFlags flags)
{
    if (encoding == kCFStringEncodingInvalidId) {
        return 0;
    }
    
    static const CFDictionaryKeyCallBacks QTextCodecKeyCallbacks = { 0, NULL, NULL, NULL, QTextCodecsEqual, QTextCodecHash };
    static CFMutableDictionaryRef encodingToCodec = CFDictionaryCreateMutable(NULL, 0, &QTextCodecKeyCallbacks, NULL);
    
    QTextCodec key(encoding, flags);
    const void *value;
    if (CFDictionaryGetValueIfPresent(encodingToCodec, &key, &value)) {
        return const_cast<QTextCodec *>(static_cast<const QTextCodec *>(value));
    }
    QTextCodec *codec = new QTextCodec(encoding, flags);
    CFDictionarySetValue(encodingToCodec, codec, codec);
    return codec;
}

QTextCodec *QTextCodec::codecForName(const char *name)
{
    KWQEncodingFlags flags;
    CFStringEncoding encoding = KWQCFStringEncodingFromIANACharsetName(name, &flags);
    return codecForCFStringEncoding(encoding, flags);
}

QTextCodec *QTextCodec::codecForNameEightBitOnly(const char *name)
{
    KWQEncodingFlags flags;
    CFStringEncoding encoding = KWQCFStringEncodingFromIANACharsetName(name, &flags);
    switch (encoding) {
        case kCFStringEncodingUnicode:
            encoding = kCFStringEncodingUTF8;
            break;
    }
    return codecForCFStringEncoding(encoding, flags);
}

QTextCodec *QTextCodec::codecForLocale()
{
    return codecForCFStringEncoding(CFStringGetSystemEncoding(), NoEncodingFlags);
}

const char *QTextCodec::name() const
{
    return KWQCFStringEncodingToIANACharsetName(_encoding);
}

QTextDecoder *QTextCodec::makeDecoder() const
{
    return new KWQTextDecoder(_encoding, _flags);
}

QCString QTextCodec::fromUnicode(const QString &qcs) const
{
    CFStringRef cfs = qcs.getCFString();
    CFRange range = CFRangeMake(0, CFStringGetLength(cfs));
    CFIndex bufferLength;
    CFStringGetBytes(cfs, range, _encoding, '?', false, NULL, 0x7FFFFFFF, &bufferLength);
    QCString result(bufferLength + 1);
    CFStringGetBytes(cfs, range, _encoding, '?', false, (UInt8 *)result.data(), bufferLength, &bufferLength);
    result[bufferLength] = 0;
    return result;
}

QString QTextCodec::toUnicode(const char *chs, int len) const
{
    return KWQTextDecoder(_encoding, _flags).toUnicode(chs, len, true);
}

QString QTextCodec::toUnicode(const QByteArray &qba, int len) const
{
    return KWQTextDecoder(_encoding, _flags).toUnicode(qba, len, true);
}

bool operator==(const QTextCodec &a, const QTextCodec &b)
{
    return a._encoding == b._encoding && a._flags == b._flags;
}

unsigned QTextCodec::hash() const
{
    unsigned h = _encoding;

    h += (h << 10);
    h ^= (h << 6);
    
    h ^= _flags;

    h += (h << 3);
    h ^= (h >> 11);
    h += (h << 15);
    
    return h;
}

static Boolean QTextCodecsEqual(const void *a, const void *b)
{
    return *static_cast<const QTextCodec *>(a) == *static_cast<const QTextCodec *>(b);
}

static CFHashCode QTextCodecHash(const void *value)
{
    return static_cast<const QTextCodec *>(value)->hash();
}

// ================

QTextDecoder::~QTextDecoder()
{
}

// ================

KWQTextDecoder::KWQTextDecoder(CFStringEncoding e, KWQEncodingFlags f)
    : _encoding(e), _flags(f), _state(atStart), _haveBufferedByte(false), _converter(0)
{
}

KWQTextDecoder::~KWQTextDecoder()
{
    if (_converter) {
        if (_cachedConverter != 0) {
            TECDisposeConverter(_cachedConverter);
        }
        _cachedConverter = _converter;
        _cachedConverterEncoding = _encoding;
    }
}

QString KWQTextDecoder::convertUTF16(const unsigned char *s, int length)
{
    ASSERT(length > 0);
    
    const unsigned char *p = s;
    unsigned len = length;
    
    // Check for the BOM.
    if (_state == atStart) {
        unsigned char bom0;
        unsigned char bom1;
        if (_haveBufferedByte) {
            bom0 = _bufferedByte;
            bom1 = p[0];
        } else {
            if (len == 1) {
                _haveBufferedByte = true;
                _bufferedByte = p[0];
                return QString::null;
            }
            bom0 = p[0];
            bom1 = p[1];
        }
        if (bom0 == 0xFF && bom1 == 0xFE) {
            _state = littleEndian;
            if (_haveBufferedByte) {
                _haveBufferedByte = false;
                p += 1;
                len -= 1;
            } else {
                p += 2;
                len -= 2;
            }
        } else if (bom0 == 0xFE && bom1 == 0xFF) {
            _state = bigEndian;
            if (_haveBufferedByte) {
                _haveBufferedByte = false;
                p += 1;
                len -= 1;
            } else {
                p += 2;
                len -= 2;
            }
        } else {
            _state = (_flags & ::LittleEndian) ? littleEndian : bigEndian;
        }
    }
    
    QString result;
    
    if (_haveBufferedByte && len) {
        UniChar c;
        if (_state == littleEndian) {
            c = _bufferedByte | (p[0] << 8);
        } else {
            c = (_bufferedByte << 8) | p[0];
        }
        result.append(reinterpret_cast<QChar *>(&c), 1);
        _haveBufferedByte = false;
        p += 1;
        len -= 1;
    }
    
    while (len > 1) {
        UniChar buffer[4096];
        int runLength = MIN(len / 2, sizeof(buffer) / sizeof(buffer[0]));
        if (_state == littleEndian) {
            for (int i = 0; i < runLength; ++i) {
                buffer[i] = p[0] | (p[1] << 8);
                p += 2;
            }
        } else {
            for (int i = 0; i < runLength; ++i) {
                buffer[i] = (p[0] << 8) | p[1];
                p += 2;
            }
        }
        result.append(reinterpret_cast<QChar *>(buffer), runLength);
        len -= runLength * 2;
    }
    
    if (len) {
        _haveBufferedByte = true;
        _bufferedByte = p[0];
    }
    
    return result;
}

QString KWQTextDecoder::convertUsingTEC(const UInt8 *chs, int len, bool flush)
{
    OSStatus status;

    // Get a converter for the passed-in encoding.
    if (!_converter) {
        if (_cachedConverterEncoding == _encoding) {
            _converter = _cachedConverter;
            _cachedConverter = 0;
            _cachedConverterEncoding = kCFStringEncodingInvalidId;
            TECClearConverterContextInfo(_converter);
        } else {
            status = TECCreateConverter(&_converter, _encoding,
                CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat));
            if (status) {
                ERROR("the Text Encoding Converter won't convert from text encoding 0x%X, error %d", _encoding, status);
                return QString::null;
            }

            // Workaround for missing TECSetBasicOptions call.
            reinterpret_cast<TECObjectPeek **>(_converter)[0]->optionsControlFlags |= kUnicodeForceASCIIRangeMask;
        }
    }
    
    QString result;

    const UInt8 *sourcePointer = chs;
    unsigned long sourceLength = len;
    
    for (;;) {
        UniChar buffer[4096];
        unsigned long bytesWritten = 0;
        bool doingFlush = false;
        
        if (sourceLength == 0) {
            if (!flush) {
                // Done.
                break;
            }
            doingFlush = true;
        }
         
        if (doingFlush) {
            status = TECFlushText(_converter,
                reinterpret_cast<UInt8 *>(buffer), sizeof(buffer), &bytesWritten);
        } else {
            unsigned long bytesRead = 0;
            status = TECConvertText(_converter, sourcePointer, sourceLength, &bytesRead,
                reinterpret_cast<UInt8 *>(buffer), sizeof(buffer), &bytesWritten);
            sourcePointer += bytesRead;
            sourceLength -= bytesRead;
        }
        if (bytesWritten) {
            ASSERT(bytesWritten % sizeof(UniChar) == 0);
            result.append(reinterpret_cast<QChar *>(buffer), bytesWritten / sizeof(UniChar));
        }
        if (status == kTextMalformedInputErr || status == kTextUndefinedElementErr) {
            // FIXME: Put in FFFD character here?
            TECClearConverterContextInfo(_converter);
            if (sourceLength) {
                sourcePointer += 1;
                sourceLength -= 1;
            }
            status = noErr;
        }
        if (status == kTECOutputBufferFullStatus) {
            continue;
        }
        if (status != noErr) {
            ERROR("text decoding failed with error %d", status);
            break;
        }
        
        if (doingFlush) {
            // Done.
            break;
        }
    }
    
    // Workaround for a bug in the Text Encoding Converter (see bug 3225472).
    // Simplified Chinese pages use the code U+A3A0 to mean "full-width space".
    // But GB18030 decodes it to U+E5E5, which is correct in theory but not in practice.
    // To work around, just change all occurences of U+E5E5 to U+3000 (ideographic space).
    if (_encoding == kCFStringEncodingGB_18030_2000) {
        result.replace(0xE5E5, 0x3000);
    }
    
    return result;
}

QString KWQTextDecoder::toUnicode(const char *chs, int len, bool flush)
{
    ASSERT_ARG(len, len >= 0);
    
    if (!chs || len <= 0) {
        return QString::null;
    }

    if (_encoding == kCFStringEncodingUnicode) {
        return convertUTF16(reinterpret_cast<const unsigned char *>(chs), len);
    }
    
    return convertUsingTEC(reinterpret_cast<const UInt8 *>(chs), len, flush);
}
