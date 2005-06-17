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

#import "KWQTextCodec.h"

#import "KWQAssertions.h"
#import "KWQCharsets.h"

const UniChar replacementCharacter = 0xFFFD;
const UniChar BOM = 0xFEFF;

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
    QString convertUsingTEC(const unsigned char *chs, int len, bool flush);
    
    OSStatus createTECConverter();
    OSStatus convertOneChunkUsingTEC(const unsigned char *inputBuffer, int inputBufferLength, int &inputLength,
        void *outputBuffer, int outputBufferLength, int &outputLength);
    static void appendOmittingUnwanted(QString &s, const UniChar *characters, int byteCount);
    
    KWQTextDecoder(const KWQTextDecoder &);
    KWQTextDecoder &operator=(const KWQTextDecoder &);

    CFStringEncoding _encoding;
    bool _littleEndian;
    bool _atStart;
    bool _error;

    unsigned _numBufferedBytes;
    unsigned char _bufferedBytes[16]; // bigger than any single multi-byte character

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
    QString copy = qcs;
    copy.replace(QChar('\\'), backslashAsCurrencySymbol());
    CFStringRef cfs = copy.getCFString();

    CFRange range = CFRangeMake(0, CFStringGetLength(cfs));
    CFIndex bufferLength;
    CFStringGetBytes(cfs, range, encoding, '?', FALSE, NULL, 0x7FFFFFFF, &bufferLength);
    QCString result(bufferLength + 1);
    CFStringGetBytes(cfs, range, encoding, '?', FALSE, reinterpret_cast<unsigned char *>(result.data()), bufferLength, &bufferLength);
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
    : _encoding(e), _littleEndian(f & ::LittleEndian), _atStart(true), _error(false)
    , _numBufferedBytes(0), _converter(0)
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
        UniChar buffer[16384];
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

OSStatus KWQTextDecoder::createTECConverter()
{
    const CFStringEncoding encoding = effectiveEncoding(_encoding);

    if (_cachedConverterEncoding == encoding) {
        _converter = _cachedConverter;
        _cachedConverter = 0;
        _cachedConverterEncoding = kCFStringEncodingInvalidId;
        TECClearConverterContextInfo(_converter);
    } else {
        OSStatus status = TECCreateConverter(&_converter, encoding,
            CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat));
        if (status) {
            ERROR("the Text Encoding Converter won't convert from text encoding 0x%X, error %d", encoding, status);
            return status;
        }

        TECSetBasicOptions(_converter, kUnicodeForceASCIIRangeMask);
    }
    
    return noErr;
}

// We strip NUL characters because other browsers (at least WinIE) do.
// We strip replacement characters because the TEC converter for UTF-8 converts
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

OSStatus KWQTextDecoder::convertOneChunkUsingTEC(const unsigned char *inputBuffer, int inputBufferLength, int &inputLength,
    void *outputBuffer, int outputBufferLength, int &outputLength)
{
    OSStatus status;
    unsigned long bytesRead = 0;
    unsigned long bytesWritten = 0;

    if (_numBufferedBytes != 0) {
        // Finish converting a partial character that's in our buffer.
        
        // First, fill the partial character buffer with as many bytes as are available.
        ASSERT(_numBufferedBytes < sizeof(_bufferedBytes));
        const int spaceInBuffer = sizeof(_bufferedBytes) - _numBufferedBytes;
        const int bytesToPutInBuffer = MIN(spaceInBuffer, inputBufferLength);
        ASSERT(bytesToPutInBuffer != 0);
        memcpy(_bufferedBytes + _numBufferedBytes, inputBuffer, bytesToPutInBuffer);

        // Now, do a conversion on the buffer.
        status = TECConvertText(_converter, _bufferedBytes, _numBufferedBytes + bytesToPutInBuffer, &bytesRead,
            reinterpret_cast<unsigned char *>(outputBuffer), outputBufferLength, &bytesWritten);
        ASSERT(bytesRead <= _numBufferedBytes + bytesToPutInBuffer);

        if (status == kTECPartialCharErr && bytesRead == 0) {
            // Handle the case where the partial character was not converted.
            if (bytesToPutInBuffer >= spaceInBuffer) {
                ERROR("TECConvertText gave a kTECPartialCharErr but read none of the %u bytes in the buffer", sizeof(_bufferedBytes));
                _numBufferedBytes = 0;
                status = kTECUnmappableElementErr; // should never happen, but use this error code
            } else {
                // Tell the caller we read all the source bytes and keep them in the buffer.
                _numBufferedBytes += bytesToPutInBuffer;
                bytesRead = bytesToPutInBuffer;
                status = noErr;
            }
        } else {
            // We are done with the partial character buffer.
            // Also, we have read some of the bytes from the main buffer.
            if (bytesRead > _numBufferedBytes) {
                bytesRead -= _numBufferedBytes;
            } else {
                ERROR("TECConvertText accepted some bytes it previously rejected with kTECPartialCharErr");
                bytesRead = 0;
            }
            _numBufferedBytes = 0;
            if (status == kTECPartialCharErr) {
                // While there may be a partial character problem in the small buffer,
                // we have to try again and not get confused and think there is a partial
                // character problem in the large buffer.
                status = noErr;
            }
        }
    } else {
        status = TECConvertText(_converter, inputBuffer, inputBufferLength, &bytesRead,
            static_cast<unsigned char *>(outputBuffer), outputBufferLength, &bytesWritten);
        ASSERT(static_cast<int>(bytesRead) <= inputBufferLength);
    }

    // Work around bug 3351093, where sometimes we get kTECBufferBelowMinimumSizeErr instead of kTECOutputBufferFullStatus.
    if (status == kTECBufferBelowMinimumSizeErr && bytesWritten != 0) {
        status = kTECOutputBufferFullStatus;
    }

    inputLength = bytesRead;
    outputLength = bytesWritten;
    return status;
}

QString KWQTextDecoder::convertUsingTEC(const unsigned char *chs, int len, bool flush)
{
    // Get a converter for the passed-in encoding.
    if (!_converter && createTECConverter() != noErr) {
        return QString();
    }
    
    QString result("");

    result.reserve(len);

    const unsigned char *sourcePointer = chs;
    int sourceLength = len;
    bool bufferWasFull = false;
    UniChar buffer[16384];

    while (sourceLength || bufferWasFull) {
        int bytesRead = 0;
        int bytesWritten = 0;
        OSStatus status = convertOneChunkUsingTEC(sourcePointer, sourceLength, bytesRead, buffer, sizeof(buffer), bytesWritten);
        ASSERT(bytesRead <= sourceLength);
        sourcePointer += bytesRead;
        sourceLength -= bytesRead;
        
        switch (status) {
            case noErr:
            case kTECOutputBufferFullStatus:
                break;
            case kTextMalformedInputErr:
            case kTextUndefinedElementErr:
                // FIXME: Put FFFD character into the output string in this case?
                TECClearConverterContextInfo(_converter);
                if (sourceLength) {
                    sourcePointer += 1;
                    sourceLength -= 1;
                }
                break;
            case kTECPartialCharErr: {
                // Put the partial character into the buffer.
                ASSERT(_numBufferedBytes == 0);
                const int bufferSize = sizeof(_numBufferedBytes);
                if (sourceLength < bufferSize) {
                    memcpy(_bufferedBytes, sourcePointer, sourceLength);
                    _numBufferedBytes = sourceLength;
                } else {
                    ERROR("TECConvertText gave a kTECPartialCharErr, but left %u bytes in the buffer", sourceLength);
                }
                sourceLength = 0;
                break;
            }
            default:
                ERROR("text decoding failed with error %d", status);
                _error = true;
                return QString();
        }

        appendOmittingUnwanted(result, buffer, bytesWritten);

        bufferWasFull = status == kTECOutputBufferFullStatus;
    }
    
    if (flush) {
        unsigned long bytesWritten = 0;
        TECFlushText(_converter, reinterpret_cast<unsigned char *>(buffer), sizeof(buffer), &bytesWritten);
        appendOmittingUnwanted(result, buffer, bytesWritten);
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
            result += convertUsingTEC(chs + i, chunkSize, flush && (i + chunkSize == len));
        }
        return result;
#else
        return convertUsingTEC(chs, len, flush);
#endif
    }
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
