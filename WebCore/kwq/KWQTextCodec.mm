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

class KWQTextDecoder : public QTextDecoder {
public:
    KWQTextDecoder(const QTextCodec &c) : _codec(c), _state(atStart), _haveBufferedByte(false) { }
    QString toUnicode(const char *chs, int len);

private:
    QString convertUTF16(const unsigned char *chs, int len);
    QString convertUsingTEC(const UInt8 *chs, int len);

    QTextCodec _codec;
    
    // State for Unicode decoding.
    enum UnicodeEndianState {
        atStart,
        littleEndian,
        bigEndian
    };
    UnicodeEndianState _state;
    bool _haveBufferedByte;
    char _bufferedByte;
};

static QTextCodec *codecForCFStringEncoding(CFStringEncoding encoding)
{
    if (encoding == kCFStringEncodingInvalidId) {
        return 0;
    }
    
    static CFMutableDictionaryRef encodingToCodec = CFDictionaryCreateMutable(NULL, 0, NULL, NULL);
    
    const void *value;
    if (CFDictionaryGetValueIfPresent(encodingToCodec, (void *)encoding, &value)) {
        return (QTextCodec *)value;
    }
    QTextCodec *codec = new QTextCodec(encoding);
    CFDictionarySetValue(encodingToCodec, (void *)encoding, codec);
    return codec;
}

QTextCodec *QTextCodec::codecForMib(int mib)
{
    return codecForCFStringEncoding(KWQCFStringEncodingFromMIB(mib));
}

QTextCodec *QTextCodec::codecForName(const char *name)
{
    return codecForCFStringEncoding(KWQCFStringEncodingFromIANACharsetName(name));
}

QTextCodec *QTextCodec::codecForLocale()
{
    return codecForCFStringEncoding(CFStringGetSystemEncoding());
}

const char *QTextCodec::name() const
{
    return KWQCFStringEncodingToIANACharsetName(_encoding);
}

int QTextCodec::mibEnum() const
{
    return KWQCFStringEncodingToMIB(_encoding);
}

QTextDecoder *QTextCodec::makeDecoder() const
{
    return new KWQTextDecoder(*this);
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
    return KWQTextDecoder(*this).toUnicode(chs, len);
}

QString QTextCodec::toUnicode(const QByteArray &qba, int len) const
{
    return KWQTextDecoder(*this).toUnicode(qba, len);
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
            _state = bigEndian;
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

QString KWQTextDecoder::convertUsingTEC(const UInt8 *chs, int len)
{
    // FIXME: This discards state between calls, which won't work for multibyte encodings.

    // Get a converter for the passed-in encoding.
    static TECObjectRef converter;
    static CFStringEncoding converterEncoding = kCFStringEncodingInvalidId;
    OSStatus status;
    if (_codec.encoding() != converterEncoding) {
        TECObjectRef newConverter;
        status = TECCreateConverter(&newConverter, _codec.encoding(),
            CreateTextEncoding(kTextEncodingUnicodeDefault, kTextEncodingDefaultVariant, kUnicode16BitFormat));
        if (status) {
            ERROR("the Text Encoding Converter won't convert from text encoding 0x%X, error %d", _codec.encoding(), status);
            return QString::null;
        }
        if (converter) {
            TECDisposeConverter(converter);
        }
        converter = newConverter;
    } else {
        TECClearConverterContextInfo(converter);
    }
    
    QString result;

    const UInt8 *sourcePointer = chs;
    unsigned long sourceLength = len;
    
    for (;;) {
        UniChar buffer[4096];
        unsigned long bytesWritten = 0;
        bool doingFlush = sourceLength == 0;
        if (doingFlush) {
            status = TECFlushText(converter,
                reinterpret_cast<UInt8 *>(buffer), sizeof(buffer), &bytesWritten);
        } else {
            unsigned long bytesRead = 0;
            status = TECConvertText(converter, sourcePointer, sourceLength, &bytesRead,
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
            TECClearConverterContextInfo(converter);
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
    
    return result;
}

QString KWQTextDecoder::toUnicode(const char *chs, int len)
{
    ASSERT_ARG(chs, chs);
    ASSERT_ARG(len, len >= 0);
    
    if (len <= 0) {
        return QString::null;
    }

    if (_codec.encoding() == kCFStringEncodingUnicode) {
        return convertUTF16(reinterpret_cast<const unsigned char *>(chs), len);
    }
    
    return convertUsingTEC(reinterpret_cast<const UInt8 *>(chs), len);
}
