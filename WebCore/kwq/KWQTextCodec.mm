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

const UniChar BOM = 0xFEFF;

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2

struct TECObjectPeek {
    UInt32 skip1;
    UInt32 skip2;
    UInt32 skip3;
    OptionBits optionsControlFlags;
};

#endif

class KWQTextDecoder : public QTextDecoder {
public:
    KWQTextDecoder(CFStringEncoding, KWQEncodingFlags);
    ~KWQTextDecoder();
    
    QString toUnicode(const char *chs, int len, bool flush);

private:
    QString convert(const char *chs, int len, bool flush);
    QString convertUTF16(const unsigned char *chs, int len);
    QString convertUsingTEC(const UInt8 *chs, int len, bool flush);
    
    KWQTextDecoder(const KWQTextDecoder &);
    KWQTextDecoder &operator=(const KWQTextDecoder &);

    CFStringEncoding _encoding;
    bool _littleEndian;
    bool _atStart;
    int _numBufferedBytes;
    char _bufferedBytes[2];
    
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

inline CFStringEncoding effectiveEncoding(CFStringEncoding e)
{
    switch (e) {
        case kCFStringEncodingISOLatin1:
        case kCFStringEncodingASCII:
            e = kCFStringEncodingWindowsLatin1;
            break;
    }
    return e;
}

QCString QTextCodec::fromUnicode(const QString &qcs) const
{
    // FIXME: We should really use the same API in both directions.
    // Currently we use TEC to decode and CFString to encode; it would be better to encode with TEC too.
    
    CFStringEncoding encoding = effectiveEncoding(_encoding);

    // FIXME: Since there's no "force ASCII range" mode in CFString, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    QString copy;
    bool usingCopy = false;
    QChar currencySymbol = backslashAsCurrencySymbol();
    if (currencySymbol != '\\' && qcs.find('\\') != -1) {
	usingCopy = true;
        copy = qcs;
	copy.replace('\\', currencySymbol);
    }

    CFStringRef cfs = usingCopy ? copy.getCFString() : qcs.getCFString();

    CFRange range = CFRangeMake(0, CFStringGetLength(cfs));
    CFIndex bufferLength;
    CFStringGetBytes(cfs, range, encoding, '?', FALSE, NULL, 0x7FFFFFFF, &bufferLength);
    QCString result(bufferLength + 1);
    CFStringGetBytes(cfs, range, encoding, '?', FALSE, reinterpret_cast<UInt8 *>(result.data()), bufferLength, &bufferLength);
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

QChar QTextCodec::backslashAsCurrencySymbol() const
{
    // FIXME: We should put this information into KWQCharsetData instead of having a switch here.
    switch (_encoding) {
        case kCFStringEncodingShiftJIS_X0213_00:
        case kCFStringEncodingEUC_JP:
            return 0x00A5; // yen sign
        default:
            return '\\';
    }
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
    : _encoding(e), _littleEndian(f & ::LittleEndian), _atStart(true), _numBufferedBytes(0), _converter(0)
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
    ASSERT(_numBufferedBytes == 0 || _numBufferedBytes == 1);
    
    const unsigned char *p = s;
    unsigned len = length;
    
    QString result;
    
    if (_numBufferedBytes != 0 && len != 0) {
        ASSERT(_numBufferedBytes == 1);
        UniChar c;
        if (_littleEndian) {
            c = _bufferedBytes[0] | (p[0] << 8);
        } else {
            c = (_bufferedBytes[0] << 8) | p[0];
        }
        result.append(reinterpret_cast<QChar *>(&c), 1);
        _numBufferedBytes = 0;
        p += 1;
        len -= 1;
    }
    
    while (len > 1) {
        UniChar buffer[4096];
        int runLength = MIN(len / 2, sizeof(buffer) / sizeof(buffer[0]));
        int bufferLength = 0;
        if (_littleEndian) {
            for (int i = 0; i < runLength; ++i) {
                UniChar c = p[0] | (p[1] << 8);
                p += 2;
                if (c != BOM) {
                    buffer[bufferLength++] = c;
                }
            }
        } else {
            for (int i = 0; i < runLength; ++i) {
                UniChar c = (p[0] << 8) | p[1];
                p += 2;
                if (c != BOM) {
                    buffer[bufferLength++] = c;
                }
            }
        }
        result.append(reinterpret_cast<QChar *>(buffer), bufferLength);
        len -= bufferLength * 2;
    }
    
    if (len) {
        ASSERT(_numBufferedBytes == 0);
        _numBufferedBytes = 1;
        _bufferedBytes[0] = p[0];
    }
    
    return result;
}

QString KWQTextDecoder::convertUsingTEC(const UInt8 *chs, int len, bool flush)
{
    OSStatus status;
    
    CFStringEncoding encoding = effectiveEncoding(_encoding);

    // Get a converter for the passed-in encoding.
    if (!_converter) {
        if (_cachedConverterEncoding == encoding) {
            _converter = _cachedConverter;
            _cachedConverter = 0;
            _cachedConverterEncoding = kCFStringEncodingInvalidId;
            TECClearConverterContextInfo(_converter);
        } else {
            status = TECCreateConverter(&_converter, encoding,
                CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat));
            if (status) {
                ERROR("the Text Encoding Converter won't convert from text encoding 0x%X, error %d", encoding, status);
                return QString();
            }

#if MAC_OS_X_VERSION_MAX_ALLOWED <= MAC_OS_X_VERSION_10_2
            // Workaround for missing TECSetBasicOptions call.
            reinterpret_cast<TECObjectPeek **>(_converter)[0]->optionsControlFlags = kUnicodeForceASCIIRangeMask;
#else
            TECSetBasicOptions(_converter, kUnicodeForceASCIIRangeMask);
#endif
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
            int start = 0;
            int characterCount = bytesWritten / sizeof(UniChar);
            for (int i = 0; i != characterCount; ++i) {
                if (buffer[i] == BOM) {
                    if (start != i) {
                        result.append(reinterpret_cast<QChar *>(&buffer[start]), i - start);
                    }
                    start = i + 1;
                }
            }
            if (start != characterCount) {
                result.append(reinterpret_cast<QChar *>(&buffer[start]), characterCount - start);
            }
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
    if (encoding == kCFStringEncodingGB_18030_2000) {
        result.replace(0xE5E5, 0x3000);
    }
    
    return result;
}

QString KWQTextDecoder::convert(const char *chs, int len, bool flush)
{
    if (_encoding == kCFStringEncodingUnicode) {
        return convertUTF16(reinterpret_cast<const unsigned char *>(chs), len);
    }
    return convertUsingTEC(reinterpret_cast<const UInt8 *>(chs), len, flush);
}

QString KWQTextDecoder::toUnicode(const char *chs, int len, bool flush)
{
    ASSERT_ARG(len, len >= 0);
    
    if (!chs || len <= 0) {
        return QString();
    }

    // Handle normal case.
    if (!_atStart) {
        return convert(chs, len, flush);
    }

    // Check to see if we found a BOM.
    int numBufferedBytes = _numBufferedBytes;
    int buf1Len = numBufferedBytes;
    int buf2Len = len;
    const char *buf1 = _bufferedBytes;
    const char *buf2 = chs;
    unsigned char c1 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c2 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    unsigned char c3 = buf1Len ? (--buf1Len, *buf1++) : buf2Len ? (--buf2Len, *buf2++) : 0;
    int BOMLength = 0;
    if (c1 == 0xFF && c2 == 0xFE) {
        _encoding = kCFStringEncodingUnicode;
        _littleEndian = true;
        BOMLength = 2;
    } else if (c1 == 0xFE && c2 == 0xFF) {
        _encoding = kCFStringEncodingUnicode;
        _littleEndian = false;
        BOMLength = 2;
    } else if (c1 == 0xEF && c2 == 0xBB && c3 == 0xBF) {
        _encoding = kCFStringEncodingUTF8;
        BOMLength = 3;
    }

    // Handle case where we found a BOM.
    if (BOMLength != 0) {
        ASSERT(numBufferedBytes + len >= BOMLength);
        int skip = BOMLength - numBufferedBytes;
        _numBufferedBytes = 0;
        _atStart = false;
        return len == skip ? QString() : convert(chs + skip, len - skip, flush);
    }

    // Handle case where we know there is no BOM coming.
    const int bufferSize = sizeof(_bufferedBytes);
    if (numBufferedBytes + len > bufferSize || flush) {
        _atStart = false;
        if (numBufferedBytes == 0) {
            return convert(chs, len, flush);
        }
        char bufferedBytes[sizeof(_bufferedBytes)];
        memcpy(bufferedBytes, _bufferedBytes, numBufferedBytes);
        _numBufferedBytes = 0;
        return convert(bufferedBytes, numBufferedBytes, false) + convert(chs, len, flush);
    }

    // Continue to look for the BOM.
    memcpy(&_bufferedBytes[numBufferedBytes], chs, len);
    _numBufferedBytes += len;
    return QString();
}
