/*
 * Copyright (C) 2004 Apple Computer, Inc.  All rights reserved.
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

#include "config.h"
#include "KWQTextCodec.h"

#include <kxmlcore/Assertions.h>
#include "KWQCharsets.h"
#include "KWQLogging.h"
#include <unicode/ucnv.h>
#include <unicode/utypes.h>

const UniChar replacementCharacter = 0xFFFD;
const UniChar BOM = 0xFEFF;

static const int ConversionBufferSize = 16384;

class KWQTextDecoder : public QTextDecoder {
public:
    KWQTextDecoder(CFStringEncoding, KWQEncodingFlags);
    ~KWQTextDecoder();
    
    QString toUnicode(const char *chs, int len, bool flush);

private:
    QString convert(const char *chs, int len, bool flush)
        { return convert(reinterpret_cast<const unsigned char *>(chs), len, flush); }
    QString convert(const unsigned char *chs, int len, bool flush);
    QString convertLatin1(const unsigned char *chs, int len);
    QString convertUTF16(const unsigned char *chs, int len);
    
    // ICU decoding.
    QString convertUsingICU(const unsigned char *chs, int len, bool flush);
    UErrorCode createICUConverter();

    static void appendOmittingUnwanted(QString &s, const UniChar *characters, int byteCount);
    
    KWQTextDecoder(const KWQTextDecoder &);
    KWQTextDecoder &operator=(const KWQTextDecoder &);

    CFStringEncoding _encoding;
    bool _littleEndian;
    bool _atStart;
    bool _error;

    unsigned _numBufferedBytes;
    unsigned char _bufferedBytes[16]; // bigger than any single multi-byte character

    // ICU decoding.
    UConverter *_converterICU;
    static UConverter *_cachedConverterICU;
    static CFStringEncoding _cachedConverterEncoding;
};

UConverter *KWQTextDecoder::_cachedConverterICU;
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

QCString QTextCodec::fromUnicode(const QString &qcs, bool allowEntities) const
{
    // FIXME: We should really use the same API in both directions.
    // Currently we use ICU to decode and CFString to encode; it would be better to encode with ICU too.
    
    CFStringEncoding encoding = effectiveEncoding(_encoding);

    // FIXME: Since there's no "force ASCII range" mode in CFString, we change the backslash into a yen sign.
    // Encoding will change the yen sign back into a backslash.
    QString copy = qcs;
    copy.replace(QChar('\\'), backslashAsCurrencySymbol());
    CFStringRef cfs = copy.getCFString();
    
    CFIndex startPos = 0;
    CFIndex charactersLeft = CFStringGetLength(cfs);
    QCString result(1); // for trailng zero

    while (charactersLeft > 0) {
        CFRange range = CFRangeMake(startPos, charactersLeft);
        CFIndex bufferLength;
        CFStringGetBytes(cfs, range, encoding, allowEntities ? 0 : '?', FALSE, NULL, 0x7FFFFFFF, &bufferLength);
        
        QCString chunk(bufferLength + 1);
        CFIndex charactersConverted = CFStringGetBytes(cfs, range, encoding, allowEntities ? 0 : '?', FALSE, reinterpret_cast<unsigned char *>(chunk.data()), bufferLength, &bufferLength);
        chunk[bufferLength] = 0;
        result.append(chunk);
        
        if (charactersConverted != charactersLeft) {
            // FIXME: support surrogate pairs
            UniChar badChar = CFStringGetCharacterAtIndex(cfs, startPos + charactersConverted);
            char buf[16];
            sprintf(buf, "&#%u;", badChar);
            result.append(buf);
            
            ++charactersConverted;
        }
        
        startPos += charactersConverted;
        charactersLeft -= charactersConverted;
    }
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

// Golden ratio - arbitrary start value to avoid mapping all 0's to all 0's
// or anything like that.
const unsigned PHI = 0x9e3779b9U;

// Paul Hsieh's SuperFastHash
// http://www.azillionmonkeys.com/qed/hash.html
// Adapted assuming _encoding is 32 bits and _flags is at most 16 bits
unsigned QTextCodec::hash() const
{
    uint32_t hash = PHI;
    uint32_t tmp;
    
    hash += _encoding & 0xffff;
    tmp = ((_encoding >> 16) << 11) ^ hash;
    hash = (hash << 16) ^ tmp;
    hash += hash >> 11;
    
    hash += _flags & 0xffff;
    hash ^= hash << 11;
    hash += hash >> 17;

    // Force "avalanching" of final 127 bits
    hash ^= hash << 3;
    hash += hash >> 5;
    hash ^= hash << 2;
    hash += hash >> 15;
    hash ^= hash << 10;

    return hash;
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
    : _encoding(e), _littleEndian(f & ::LittleEndian), _atStart(true), _error(false)
    , _numBufferedBytes(0), _converterICU(0)
{
}

KWQTextDecoder::~KWQTextDecoder()
{
    if (_converterICU) {
        if (_cachedConverterICU != 0) {
            ucnv_close(_cachedConverterICU);
        }
        _cachedConverterICU = _converterICU;
        _cachedConverterEncoding = _encoding;
    }
}

QString KWQTextDecoder::convertLatin1(const unsigned char *s, int length)
{
    ASSERT(_numBufferedBytes == 0);

    int i;
    for (i = 0; i != length; ++i) {
        if (s[i] == 0) {
            break;
        }
    }
    if (i == length) {
        return QString(reinterpret_cast<const char *>(s), length);
    }

    QString result("");
    
    result.reserve(length);
    
    result.append(reinterpret_cast<const char *>(s), i);
    int start = ++i;
    for (; i != length; ++i) {
        if (s[i] == 0) {
            if (start != i) {
                result.append(reinterpret_cast<const char *>(&s[start]), i - start);
            }
            start = i + 1;
        }
    }
    if (start != length) {
        result.append(reinterpret_cast<const char *>(&s[start]), length - start);
    }

    return result;
}

QString KWQTextDecoder::convertUTF16(const unsigned char *s, int length)
{
    ASSERT(_numBufferedBytes == 0 || _numBufferedBytes == 1);

    const unsigned char *p = s;
    unsigned len = length;
    
    QString result("");
    
    result.reserve(length / 2);

    if (_numBufferedBytes != 0 && len != 0) {
        ASSERT(_numBufferedBytes == 1);
        UniChar c;
        if (_littleEndian) {
            c = _bufferedBytes[0] | (p[0] << 8);
        } else {
            c = (_bufferedBytes[0] << 8) | p[0];
        }
        if (c) {
            result.append(reinterpret_cast<QChar *>(&c), 1);
        }
        _numBufferedBytes = 0;
        p += 1;
        len -= 1;
    }
    
    while (len > 1) {
        UniChar buffer[ConversionBufferSize];
        int runLength = MIN(len / 2, sizeof(buffer) / sizeof(buffer[0]));
        int bufferLength = 0;
        if (_littleEndian) {
            for (int i = 0; i < runLength; ++i) {
                UniChar c = p[0] | (p[1] << 8);
                p += 2;
                if (c && c != BOM) {
                    buffer[bufferLength++] = c;
                }
            }
        } else {
            for (int i = 0; i < runLength; ++i) {
                UniChar c = (p[0] << 8) | p[1];
                p += 2;
                if (c && c != BOM) {
                    buffer[bufferLength++] = c;
                }
            }
        }
        result.append(reinterpret_cast<QChar *>(buffer), bufferLength);
        len -= runLength * 2;
    }
    
    if (len) {
        ASSERT(_numBufferedBytes == 0);
        _numBufferedBytes = 1;
        _bufferedBytes[0] = p[0];
    }
    
    return result;
}

UErrorCode KWQTextDecoder::createICUConverter()
{
    const CFStringEncoding encoding = effectiveEncoding(_encoding);
    const char *encodingName = KWQCFStringEncodingToIANACharsetName(encoding);

    bool cachedEncodingEqual = _cachedConverterEncoding == encoding;
    _cachedConverterEncoding = kCFStringEncodingInvalidId;

    if (cachedEncodingEqual && _cachedConverterICU) {
        _converterICU = _cachedConverterICU;
        _cachedConverterICU = 0;
        LOG(TextConversion, "using cached ICU converter for encoding: %s", encodingName);
    } else {    
        UErrorCode err = U_ZERO_ERROR;
        ASSERT(!_converterICU);
        LOG(TextConversion, "creating ICU converter for encoding: %s", encodingName);
        _converterICU = ucnv_open(encodingName, &err);
        if (err == U_AMBIGUOUS_ALIAS_WARNING) {
            ERROR("ICU ambiguous alias warning for encoding: %s", encodingName);
        }
        if (!_converterICU) {
            ERROR("the ICU Converter won't convert from text encoding 0x%X, error %d", encoding, err);
            return err;
        }
    }
    
    return U_ZERO_ERROR;
}

// We strip NUL characters because other browsers (at least WinIE) do.
// We strip replacement characters because the ICU converter for UTF-8 converts
// invalid sequences into replacement characters, but other browsers discard them.
// We strip BOM characters because they can show up both at the start of content
// and inside content, and we never want them to end up in the decoded text.
static inline bool unwanted(UniChar c)
{
    switch (c) {
        case 0:
        case replacementCharacter:
        case BOM:
            return true;
        default:
            return false;
    }
}

void KWQTextDecoder::appendOmittingUnwanted(QString &s, const UniChar *characters, int byteCount)
{
    ASSERT(byteCount % sizeof(UniChar) == 0);
    int start = 0;
    int characterCount = byteCount / sizeof(UniChar);
    for (int i = 0; i != characterCount; ++i) {
        if (unwanted(characters[i])) {
            if (start != i) {
                s.append(reinterpret_cast<const QChar *>(&characters[start]), i - start);
            }
            start = i + 1;
        }
    }
    if (start != characterCount) {
        s.append(reinterpret_cast<const QChar *>(&characters[start]), characterCount - start);
    }
}

QString KWQTextDecoder::convertUsingICU(const unsigned char *chs, int len, bool flush)
{
    // Get a converter for the passed-in encoding.
    if (!_converterICU && U_FAILURE(createICUConverter())) {
        return QString();
    }
    ASSERT(_converterICU);

    QString result("");
    result.reserve(len);

    UChar buffer[ConversionBufferSize];
    const char *source = reinterpret_cast<const char *>(chs);
    const char *sourceLimit = source + len;
    int32_t *offsets = NULL;
    UErrorCode err;
    
    do {
        UChar *target = buffer;
        const UChar *targetLimit = target + ConversionBufferSize;
        err = U_ZERO_ERROR;
        ucnv_toUnicode(_converterICU, &target, targetLimit, &source, sourceLimit, offsets, flush, &err);
        int count = target - buffer;
        appendOmittingUnwanted(result, reinterpret_cast<const UniChar *>(buffer), count * sizeof(UniChar));
    } while (err == U_BUFFER_OVERFLOW_ERROR);

    if (U_FAILURE(err)) {
        // flush the converter so it can be reused, and not be bothered by this error.
        do {
            UChar *target = buffer;
            const UChar *targetLimit = target + ConversionBufferSize;
            err = U_ZERO_ERROR;
            ucnv_toUnicode(_converterICU, &target, targetLimit, &source, sourceLimit, offsets, true, &err);
        } while (source < sourceLimit);
        ERROR("ICU conversion error");
        return QString();
    }
    
    return result;
}

QString KWQTextDecoder::convert(const unsigned char *chs, int len, bool flush)
{
    //#define PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE 1000

    switch (_encoding) {
    case kCFStringEncodingISOLatin1:
    case kCFStringEncodingWindowsLatin1:
        return convertLatin1(chs, len);

    case kCFStringEncodingUnicode:
        return convertUTF16(chs, len);

    default:
#if PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE
        QString result;
        int chunkSize;
        for (int i = 0; i != len; i += chunkSize) {
            chunkSize = len - i;
            if (chunkSize > PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE) {
                chunkSize = PARTIAL_CHARACTER_HANDLING_TEST_CHUNK_SIZE;
            }
            result += convertUsingICU(chs + i, chunkSize, flush && (i + chunkSize == len));
        }
        return result;
#else
        return convertUsingICU(chs, len, flush);
#endif
    }
    ASSERT_NOT_REACHED();
    return QString();
}

QString KWQTextDecoder::toUnicode(const char *chs, int len, bool flush)
{
    ASSERT_ARG(len, len >= 0);
    
    if (_error || !chs) {
        return QString();
    }
    if (len <= 0 && !flush) {
        return "";
    }

    // Handle normal case.
    if (!_atStart) {
        return convert(chs, len, flush);
    }

    // Check to see if we found a BOM.
    int numBufferedBytes = _numBufferedBytes;
    int buf1Len = numBufferedBytes;
    int buf2Len = len;
    const unsigned char *buf1 = _bufferedBytes;
    const unsigned char *buf2 = reinterpret_cast<const unsigned char *>(chs);
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
        return len == skip ? QString("") : convert(chs + skip, len - skip, flush);
    }

    // Handle case where we know there is no BOM coming.
    const int bufferSize = sizeof(_bufferedBytes);
    if (numBufferedBytes + len > bufferSize || flush) {
        _atStart = false;
        if (numBufferedBytes == 0) {
            return convert(chs, len, flush);
        }
        unsigned char bufferedBytes[sizeof(_bufferedBytes)];
        memcpy(bufferedBytes, _bufferedBytes, numBufferedBytes);
        _numBufferedBytes = 0;
        return convert(bufferedBytes, numBufferedBytes, false) + convert(chs, len, flush);
    }

    // Continue to look for the BOM.
    memcpy(&_bufferedBytes[numBufferedBytes], chs, len);
    _numBufferedBytes += len;
    return "";
}
