/*
 * Copyright (C) 2004, 2005, 2006 Apple Computer, Inc.  All rights reserved.
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
#include "QString.h"

#include "Logging.h"
#include "KWQRegExp.h"
#include "TextEncoding.h"
#include <kjs/dtoa.h>
#include <stdio.h>
#include <stdarg.h>
#ifdef WIN32
#include "Windows.h"
#endif

using namespace WebCore;

#define CHECK_FOR_HANDLE_LEAKS 0

#define ALLOC_QCHAR_GOOD_SIZE(X) (X)
#define ALLOC_CHAR_GOOD_SIZE(X) (X)

#define ALLOC_CHAR(N) (char*)fastMalloc(N)
#define REALLOC_CHAR(P, N) (char *)fastRealloc(P, N)
#define DELETE_CHAR(P) fastFree(P)

#define ALLOC_QCHAR(N) (QChar*)fastMalloc(sizeof(QChar)*(N))
#define REALLOC_QCHAR(P, N) (QChar *)fastRealloc(P, sizeof(QChar)*(N))
#define DELETE_QCHAR(P) fastFree(P)

struct HandleNode;
struct HandlePageNode;

static HandleNode *allocateNode(HandlePageNode *pageNode);
static HandlePageNode *allocatePageNode();

static HandlePageNode *usedNodeAllocationPages = 0;
static HandlePageNode *freeNodeAllocationPages = 0;

static inline void initializeHandleNodes()
{
    if (freeNodeAllocationPages == 0)
        freeNodeAllocationPages = allocatePageNode();
}

static inline KWQStringData **allocateHandle()
{
#if CHECK_FOR_HANDLE_LEAKS
    return static_cast<KWQStringData **>(fastMalloc(sizeof(KWQStringData *)));
#endif

    initializeHandleNodes();
    
    return reinterpret_cast<KWQStringData **>(allocateNode(freeNodeAllocationPages));
}

static void freeHandle(KWQStringData **);

#define IS_ASCII_QCHAR(c) ((c).unicode() > 0 && (c).unicode() <= 0xff)

static const int caseDelta = ('a' - 'A');

const char * const QString::null = 0;

KWQStringData *QString::shared_null = 0;
KWQStringData **QString::shared_null_handle = 0;

// -------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------

static inline int ucstrcmp( const QString &as, const QString &bs )
{
    const QChar *a = as.unicode();
    const QChar *b = bs.unicode();
    if ( a == b )
        return 0;
    if ( a == 0 )
        return 1;
    if ( b == 0 )
        return -1;
    int l = kMin(as.length(), bs.length());
    while ( l-- && *a == *b )
        a++,b++;
    if ( l == -1 )
        return ( as.length() - bs.length() );
    return a->unicode() - b->unicode();
}


static bool equal(const QChar *a, const char *b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (*a != *b)
            return false;
        a++; b++;
    }
    return true;
}

// Not a "true" case insensitive compare; only insensitive for plain ASCII.

static bool equalCaseInsensitive(const char *a, const char *b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (tolower(*a) != tolower(*b))
            return false;
        a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const QChar *a, const char *b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (tolower(a->unicode()) != tolower(*b))
            return false;
        a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const QChar *a, const QChar *b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (tolower(a->unicode()) != tolower(b->unicode()))
            return false;
        a++; b++;
    }
    return true;
}

static inline bool equalCaseInsensitive(char c1, char c2)
{
    return tolower(c1) == tolower(c2);
}

static inline bool equalCaseInsensitive(QChar c1, char c2)
{
    return tolower(c1.unicode()) == tolower(static_cast<unsigned char>(c2));
}

static bool ok_in_base(QChar c, int base)
{
    int uc = c.unicode();
    if (isdigit(uc))
        return uc - '0' < base;
    if (isalpha(uc)) {
        if (base > 36)
            base = 36;
        return (uc >= 'a' && uc < 'a' + base - 10)
            || (uc >= 'A' && uc < 'A' + base - 10);
    }
    return false;
}

// -------------------------------------------------------------------------
// KWQStringData
// -------------------------------------------------------------------------

// FIXME, make constructor explicity take a 'copy' flag.
// This can be used to hand off ownership of allocated data when detaching and
// deleting QStrings.

KWQStringData::KWQStringData() :
        refCount(1), _length(0), _unicode(0), _ascii(0), _maxUnicode(QS_INTERNAL_BUFFER_UCHARS), _isUnicodeValid(0), _isHeapAllocated(0), _maxAscii(QS_INTERNAL_BUFFER_CHARS), _isAsciiValid(1) 
{ 
    _ascii = _internalBuffer;
    _internalBuffer[0] = 0;
}

void KWQStringData::initialize()
{
    refCount = 1;
    _length = 0;
    _unicode = 0;
    _ascii = _internalBuffer;
    _maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
    _isUnicodeValid = 0;
    _maxAscii = QS_INTERNAL_BUFFER_CHARS;
    _isAsciiValid = 1;
    _internalBuffer[0] = 0;
    _isHeapAllocated = 0;
}

// Don't copy data.
KWQStringData::KWQStringData(QChar *u, uint l, uint m) :
        refCount(1), _length(l), _unicode(u), _ascii(0), _maxUnicode(m), _isUnicodeValid(1), _isHeapAllocated(0), _maxAscii(QS_INTERNAL_BUFFER_CHARS), _isAsciiValid(0)
{
    ASSERT(m >= l);
}

// Don't copy data.
void KWQStringData::initialize(QChar *u, uint l, uint m)
{
    ASSERT(m >= l);
    refCount = 1;
    _length = l;
    _unicode = u;
    _ascii = 0;
    _maxUnicode = m;
    _isUnicodeValid = 1;
    _maxAscii = 0;
    _isAsciiValid = 0;
    _isHeapAllocated = 0;
}

// Copy data
KWQStringData::KWQStringData(const QChar *u, uint l)
{
    initialize (u, l);
}

// Copy data
void KWQStringData::initialize(const QChar *u, uint l)
{
    refCount = 1;
    _length = l;
    _ascii = 0;
    _isUnicodeValid = 1;
    _maxAscii = 0;
    _isAsciiValid = 0;
    _isHeapAllocated = 0;

    if (l > QS_INTERNAL_BUFFER_UCHARS) {
        _maxUnicode = ALLOC_QCHAR_GOOD_SIZE(l);
        _unicode = ALLOC_QCHAR(_maxUnicode);
        memcpy(_unicode, u, l*sizeof(QChar));
    } else {
        _maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
        _unicode = (QChar *)_internalBuffer;
        if (l)
            memcpy(_internalBuffer, u, l*sizeof(QChar));
    }
}


// Copy data
KWQStringData::KWQStringData(const char *a, uint l)
{
    initialize(a, l);
}


// Copy data
void KWQStringData::initialize(const char *a, uint l)
{
    refCount = 1;
    _length = l;
    _unicode = 0;
    _isUnicodeValid = 0;
    _maxUnicode = 0;
    _isAsciiValid = 1;
    _isHeapAllocated = 0;

    if (l > QS_INTERNAL_BUFFER_CHARS) {
        _maxAscii = ALLOC_CHAR_GOOD_SIZE(l+1);
        _ascii = ALLOC_CHAR(_maxAscii);
        if (a)
            memcpy(_ascii, a, l);
        _ascii[l] = 0;
    } else {
        _maxAscii = QS_INTERNAL_BUFFER_CHARS;
        _ascii = _internalBuffer;
        if (a)
            memcpy(_internalBuffer, a, l);
        _internalBuffer[l] = 0;
    }
}

KWQStringData::KWQStringData(KWQStringData &o)
    : refCount(1)
    , _length(o._length)
    , _unicode(o._unicode)
    , _ascii(o._ascii)
    , _maxUnicode(o._maxUnicode)
    , _isUnicodeValid(o._isUnicodeValid)
    , _isHeapAllocated(0)
    , _maxAscii(o._maxAscii)
    , _isAsciiValid(o._isAsciiValid)
{
    // Handle the case where either the Unicode or 8-bit pointer was
    // pointing to the internal buffer. We need to point at the
    // internal buffer in the new object, and copy the characters.
    if (_unicode == reinterpret_cast<QChar *>(o._internalBuffer)) {
        if (_isUnicodeValid) {
            ASSERT(!_isAsciiValid || _ascii != o._internalBuffer);
            ASSERT(_length <= QS_INTERNAL_BUFFER_UCHARS);
            memcpy(_internalBuffer, o._internalBuffer, _length * sizeof(QChar));
            _unicode = reinterpret_cast<QChar *>(_internalBuffer);
        } else {
            _unicode = 0;
        }
    }
    if (_ascii == o._internalBuffer) {
        if (_isAsciiValid) {
            ASSERT(_length <= QS_INTERNAL_BUFFER_CHARS);
            memcpy(_internalBuffer, o._internalBuffer, _length);
            _internalBuffer[_length] = 0;
            _ascii = _internalBuffer;
        } else {
            _ascii = 0;
        }
    }

    // Clean up the other KWQStringData just enough so that it can be destroyed
    // cleanly. It's not in a good enough state to use, but that's OK. It just
    // needs to be in a state where ~KWQStringData won't do anything harmful,
    // and setting these to 0 will do that (preventing any double-free problems).
    o._unicode = 0;
    o._ascii = 0;
}

KWQStringData *QString::makeSharedNull()
{
    if (!shared_null) {
        shared_null = new KWQStringData;
        shared_null->ref();
        shared_null->_maxAscii = 0;
        shared_null->_maxUnicode = 0;
        shared_null->_unicode = (QChar *)&shared_null->_internalBuffer[0]; 
        shared_null->_isUnicodeValid = 1;   
    }
    return shared_null;
}

KWQStringData **QString::makeSharedNullHandle()
{
    if (!shared_null_handle) {
        shared_null_handle = allocateHandle();
        *shared_null_handle = makeSharedNull();
    }
    return shared_null_handle;
}

KWQStringData::~KWQStringData()
{
    ASSERT(refCount == 0);
    if (_unicode && !isUnicodeInternal())
        DELETE_QCHAR(_unicode);
    if (_ascii && !isAsciiInternal())
        DELETE_CHAR(_ascii);
}

void KWQStringData::increaseAsciiSize(uint size)
{
    ASSERT(this != QString::shared_null);
        
    uint newSize = (uint)ALLOC_CHAR_GOOD_SIZE((size * 3 + 1) / 2);
    
    if (!_isAsciiValid)
        makeAscii();
    ASSERT(_isAsciiValid);
    
    if (isAsciiInternal()) {
        char *newAscii = ALLOC_CHAR(newSize);
        if (_length)
            memcpy(newAscii, _ascii, _length);
        _ascii = newAscii;
    } else {
        _ascii = REALLOC_CHAR(_ascii, newSize);
    }
    
    _maxAscii = newSize;
    _isAsciiValid = 1;
    _isUnicodeValid = 0;
}


void KWQStringData::increaseUnicodeSize(uint size)
{
    ASSERT(size > _length);
    ASSERT(this != QString::shared_null);
        
    uint newSize = (uint)ALLOC_QCHAR_GOOD_SIZE((size * 3 + 1) / 2);
    
    if (!_isUnicodeValid)
        makeUnicode();
    ASSERT(_isUnicodeValid);

    if (isUnicodeInternal()) {
        QChar *newUni = ALLOC_QCHAR(newSize);
        if (_length)
            memcpy(newUni, _unicode, _length*sizeof(QChar));
        _unicode = newUni;
    } else {
        _unicode = REALLOC_QCHAR(_unicode, newSize);
    }
    
    _maxUnicode = newSize;
    _isUnicodeValid = 1;
    _isAsciiValid = 0;
}


char *KWQStringData::makeAscii()
{
    ASSERT(this != QString::shared_null);
        
    if (_isUnicodeValid){
        QChar copyBuf[QS_INTERNAL_BUFFER_CHARS];
        QChar *str;
        
        if (_ascii && !isAsciiInternal())
            DELETE_QCHAR(_ascii);
            
        if (_length < QS_INTERNAL_BUFFER_CHARS){
            if (isUnicodeInternal()) {
                uint i = _length;
                QChar *tp = &copyBuf[0], *fp = _unicode;
                while (i--)
                    *tp++ = *fp++;
                str = &copyBuf[0];
                _isUnicodeValid = 0;
            }
            else
                str = _unicode;
            _ascii = _internalBuffer;
            _maxAscii = QS_INTERNAL_BUFFER_CHARS;
        }
        else {
            uint newSize = ALLOC_CHAR_GOOD_SIZE(_length+1);
            _ascii = ALLOC_CHAR(newSize);
            _maxAscii = newSize;
            str = _unicode;
        }

        uint i = _length;
        char *cp = _ascii;
        while ( i-- )
            *cp++ = *str++;
        *cp = 0;
        
        _isAsciiValid = 1;
    }
    else if (!_isAsciiValid)
        FATAL("ASCII character cache not valid");
        
    return _ascii;
}


QChar *KWQStringData::makeUnicode()
{
    ASSERT(this != QString::shared_null);
        
    if (_isAsciiValid){
        char copyBuf[QS_INTERNAL_BUFFER_CHARS];
        char *str;
        
        if (_unicode && !isUnicodeInternal())
            DELETE_QCHAR(_unicode);
            
        if (_length <= QS_INTERNAL_BUFFER_UCHARS){
            if (isAsciiInternal()) {
                uint i = _length;
                char *tp = &copyBuf[0], *fp = _ascii;
                while (i--)
                    *tp++ = *fp++;
                str = &copyBuf[0];
                _isAsciiValid = 0;
            }
            else
                str = _ascii;
            _unicode = (QChar *)_internalBuffer;
            _maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
        }
        else {
            uint newSize = ALLOC_QCHAR_GOOD_SIZE(_length);
            _unicode = ALLOC_QCHAR(newSize);
            _maxUnicode = newSize;
            str = _ascii;
        }
        uint i = _length;
        QChar *cp = _unicode;
        while ( i-- )
            *cp++ = *str++;
        
        _isUnicodeValid = 1;
    }
    else if (!_isUnicodeValid)
        FATAL("invalid character cache");

    return _unicode;
}


// -------------------------------------------------------------------------
// QString
// -------------------------------------------------------------------------


QString QString::number(int n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(uint n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(long n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(unsigned long n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

QString QString::number(double n)
{
    QString qs;
    qs.setNum(n);
    return qs;
}

inline void QString::detachIfInternal()
{
    KWQStringData *oldData = *dataHandle;
    if (oldData->refCount > 1 && oldData == &internalData) {
        KWQStringData *newData = new KWQStringData(*oldData);
        newData->_isHeapAllocated = 1;
        newData->refCount = oldData->refCount;
        oldData->refCount = 1;
        oldData->deref();
        *dataHandle = newData;    
    }
}

const QChar *QString::stableUnicode()
{
    // if we're using the internal data of another string, detach now
    if (!dataHandle[0]->_isHeapAllocated && *dataHandle != &internalData) {
        detach();
    }
    return unicode();
}


QString::~QString()
{
    ASSERT(dataHandle);
    ASSERT(dataHandle[0]->refCount != 0);

    // Only free the handle if no other string has a reference to the
    // data.  The handle will be freed by the string that has the
    // last reference to data.
    bool needToFreeHandle = dataHandle[0]->refCount == 1 && *dataHandle != shared_null;

    // Copy our internal data if necessary, other strings still need it.
    detachIfInternal();
    
    // Remove our reference. This should always be the last reference
    // if *dataHandle points to our internal KWQStringData. If we just detached,
    // this will remove the extra ref from the new handle.
    dataHandle[0]->deref();

    ASSERT(*dataHandle != &internalData || dataHandle[0]->refCount == 0);
    
    if (needToFreeHandle)
        freeHandle(dataHandle);

#ifndef NDEBUG
    dataHandle = 0;
#endif
}


QString::QString()
{
    internalData.deref();
    dataHandle = makeSharedNullHandle();
    dataHandle[0]->ref();
}


// Careful, just used by QConstString
QString::QString(KWQStringData *constData, bool /*dummy*/) 
{
    internalData.deref();
    dataHandle = allocateHandle();
    *dataHandle = constData;
    
    // The QConstString constructor allocated the KWQStringData.
    constData->_isHeapAllocated = 1;
}


QString::QString(QChar qc)
{
    dataHandle = allocateHandle();

    // Copy the QChar.
    if (IS_ASCII_QCHAR(qc)) {
        char c = (char)qc; 
        *dataHandle = &internalData;
        internalData.initialize( &c, 1 );
    }
    else {
        *dataHandle = &internalData;
        internalData.initialize( &qc, 1 );
    }
}

QString::QString(const ByteArray &qba)
{
    dataHandle = allocateHandle();

    // Copy data
    *dataHandle = &internalData;
    internalData.initialize(qba.data(),qba.size());
}

QString::QString(const QChar *unicode, uint length)
{
    if (!unicode && !length) {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else {
        dataHandle = allocateHandle();

        // Copy the QChar *
        *dataHandle = &internalData;
        internalData.initialize(unicode, length);
    }
}

QString::QString(const char *chs)
{
    if (chs) {
        internalData.initialize(chs,strlen(chs));
        dataHandle = allocateHandle();
        *dataHandle = &internalData;
    } else {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    }
}

QString::QString(const char *chs, int len)
{
    dataHandle = allocateHandle();
    *dataHandle = &internalData;
    internalData.initialize(chs,len);
}

QString::QString(const QString &qs) : dataHandle(qs.dataHandle)
{
    internalData.deref();
    dataHandle[0]->ref();
}

QString &QString::operator=(const QString &qs)
{
    if (this == &qs)
        return *this;

    // Free our handle if it isn't the shared null handle, and if no-one else is using it.
    bool needToFreeHandle = dataHandle != shared_null_handle && dataHandle[0]->refCount == 1;
    
    qs.dataHandle[0]->ref();
    deref();
    
    if (needToFreeHandle)
        freeHandle(dataHandle);
        
    dataHandle = qs.dataHandle;

    return *this;
}

QString &QString::operator=(const QCString &qcs)
{
    return setLatin1(qcs);
}

QString &QString::operator=(const char *chs)
{
    return setLatin1(chs);
}

QString &QString::operator=(QChar qc)
{
    return *this = QString(qc);
}

QString &QString::operator=(char ch)
{
    return *this = QString(QChar(ch));
}

QChar QString::at(uint i) const
{
    KWQStringData *thisData = *dataHandle;
    
    if (i >= thisData->_length)
        return QChar::null;
        
    if (thisData->_isAsciiValid) {
        return thisData->_ascii[i];
    }
    
    ASSERT(thisData->_isUnicodeValid);
    return thisData->_unicode[i];
}

int QString::compare(const QString& s) const
{
    if (dataHandle[0]->_isAsciiValid && s.dataHandle[0]->_isAsciiValid)
        return strcmp(ascii(), s.ascii());
    return ucstrcmp(*this,s);
}

int QString::compare(const char *chs) const
{
    if (!chs)
        return isEmpty() ? 0 : 1;
    KWQStringData *d = dataHandle[0];
    if (d->_isAsciiValid)
        return strcmp(ascii(), chs);
    const QChar *s = unicode();
    uint len = d->_length;
    for (uint i = 0; i != len; ++i) {
        char c2 = chs[i];
        if (!c2)
            return 1;
        QChar c1 = s[i];
        if (c1 < c2)
            return -1;
        if (c1 > c2)
            return 1;
    }
    return chs[len] ? -1 : 0;
}

bool QString::startsWith( const QString& s ) const
{
    if (dataHandle[0]->_isAsciiValid){
        const char *asc = ascii();
        
        for ( int i =0; i < (int) s.dataHandle[0]->_length; i++ ) {
            if ( i >= (int) dataHandle[0]->_length || asc[i] != s[i] )
                return false;
        }
    }
    else if (dataHandle[0]->_isUnicodeValid){
        const QChar *uni = unicode();
        
        for ( int i =0; i < (int) s.dataHandle[0]->_length; i++ ) {
            if ( i >= (int) dataHandle[0]->_length || uni[i] != s[i] )
                return false;
        }
    }
    else
        FATAL("invalid character cache");
        
    return true;
}

bool QString::startsWith(const char *prefix) const
{
    KWQStringData *data = *dataHandle;

    uint prefixLength = strlen(prefix);
    if (data->_isAsciiValid) {
        return strncmp(prefix, data->_ascii, prefixLength) == 0;
    } else {
        ASSERT(data->_isUnicodeValid);
        if (prefixLength > data->_length) {
            return false;
        }
        const QChar *uni = data->_unicode;        
        for (uint i = 0; i < prefixLength; ++i) {
            if (uni[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }
}

#ifdef WIN32
inline int strncasecmp(const char *first, const char *second, size_t maxLength)
{
    return _strnicmp(first, second, maxLength);
}
#endif

bool QString::startsWith(const char *prefix, bool caseSensitive) const
{
    if (caseSensitive) {
        return startsWith(prefix);
    }

    KWQStringData *data = *dataHandle;

    uint prefixLength = strlen(prefix);
    if (data->_isAsciiValid) {
        return strncasecmp(prefix, data->_ascii, prefixLength) == 0;
    } else {
        ASSERT(data->_isUnicodeValid);
        if (prefixLength > data->_length) {
            return false;
        }
        const QChar *uni = data->_unicode;        
        for (uint i = 0; i < prefixLength; ++i) {
            if (!equalCaseInsensitive(uni[i], prefix[i])) {
                return false;
            }
        }
        return true;
    }
}

bool QString::endsWith(const QString& s) const
{
    const QChar *uni = unicode();

    int length = dataHandle[0]->_length;
    int slength = s.dataHandle[0]->_length;
    if (length < slength)
        return false;

    for (int i = length - slength, j = 0; i < length; i++, j++) {
        if (uni[i] != s[j])
            return false;
    }

    return true;
}

bool QString::isNull() const
{
    return dataHandle == shared_null_handle;
}

int QString::find(QChar qc, int index) const
{
    if (dataHandle[0]->_isAsciiValid) {
        if (!IS_ASCII_QCHAR(qc)) {
            return -1;
        }
        return find((char)qc, index);
    }
    return find(QString(qc), index, true);
}

int QString::find(char ch, int index) const
{
    if (dataHandle[0]->_isAsciiValid){
        const char *cp = ascii();
        
        if ( index < 0 )
            index += dataHandle[0]->_length;
        
        if (index >= (int)dataHandle[0]->_length)
            return -1;
            
        for (int i = index; i < (int)dataHandle[0]->_length; i++)
            if (cp[i] == ch)
                return i;
    }
    else if (dataHandle[0]->_isUnicodeValid)
        return find(QChar(ch), index, true);
    else
        FATAL("invalid character cache");

    return -1;
}

int QString::find(const QString &str, int index, bool caseSensitive) const
{
    // FIXME, use the first character algorithm
    /*
      We use some weird hashing for efficiency's sake.  Instead of
      comparing strings, we compare the sum of str with that of
      a part of this QString.  Only if that matches, we call memcmp
      or ucstrnicmp.

      The hash value of a string is the sum of the cells of its
      QChars.
    */
    if ( index < 0 )
        index += dataHandle[0]->_length;
    int lstr = str.dataHandle[0]->_length;
    int lthis = dataHandle[0]->_length - index;
    if ( (uint)lthis > dataHandle[0]->_length )
        return -1;
    int delta = lthis - lstr;
    if ( delta < 0 )
        return -1;

    const QChar *uthis = unicode() + index;
    const QChar *ustr = str.unicode();
    uint hthis = 0;
    uint hstr = 0;
    int i;
    if ( caseSensitive ) {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[i].unicode();
            hstr += ustr[i].unicode();
        }
        i = 0;
        while ( true ) {
            if ( hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(QChar)) == 0 )
                return index + i;
            if ( i == delta )
                return -1;
            hthis += uthis[i + lstr].unicode();
            hthis -= uthis[i].unicode();
            i++;
        }
    } else {
        for ( i = 0; i < lstr; i++ ) {
            hthis += tolower(uthis[i].unicode());
            hstr += tolower(ustr[i].unicode());
        }
        i = 0;
        while ( true ) {
            if ( hthis == hstr && equalCaseInsensitive(uthis + i, ustr, lstr) )
                return index + i;
            if ( i == delta )
                return -1;
            hthis += tolower(uthis[i + lstr].unicode());
            hthis -= tolower(uthis[i].unicode());
            i++;
        }
    }
}

// This function should be as fast as possible, every little bit helps.
// Our usage patterns are typically small strings.  In time trials
// this simplistic algorithm is much faster than Boyer-Moore or hash
// based algorithms.
int QString::find(const char *chs, int index, bool caseSensitive) const
{
    if (!chs || index < 0)
        return -1;

    KWQStringData *data = *dataHandle;

    int chsLength = strlen(chs);
    int n = data->_length - index;
    if (n < 0)
        return -1;
    n -= chsLength - 1;
    if (n <= 0)
        return -1;

    const char *chsPlusOne = chs + 1;
    int chsLengthMinusOne = chsLength - 1;

    if (data->_isAsciiValid) {
        char *ptr = data->_ascii + index - 1;
        if (caseSensitive) {
            char c = *chs;
            do {
                if (*++ptr == c && memcmp(ptr + 1, chsPlusOne, chsLengthMinusOne) == 0) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        } else {
            int lc = tolower(*chs);
            do {
                if (tolower(*++ptr) == lc && equalCaseInsensitive(ptr + 1, chsPlusOne, chsLengthMinusOne)) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        }
    } else {
        ASSERT(data->_isUnicodeValid);

        const QChar *ptr = data->_unicode + index - 1;
        if (caseSensitive) {
            QChar c = *chs;
            do {
                if (*++ptr == c && equal(ptr + 1, chsPlusOne, chsLengthMinusOne)) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        } else {
            int lc = tolower((unsigned char)*chs);
            do {
                if (tolower((++ptr)->unicode()) == lc && equalCaseInsensitive(ptr + 1, chsPlusOne, chsLengthMinusOne)) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        }
    }

    return -1;
}

int QString::find(const QRegExp &qre, int index) const
{
    if ( index < 0 )
        index += dataHandle[0]->_length;
    return qre.match( *this, index );
}

int QString::findRev(char ch, int index) const
{
    if (dataHandle[0]->_isAsciiValid){
        const char *cp = ascii();
        
        if (index < 0)
            index += dataHandle[0]->_length;
        if (index > (int)dataHandle[0]->_length)
            return -1;
            
        for (int i = index; i >= 0; i--) {
            if (cp[i] == ch)
                return i;
        }
    }
    else if (dataHandle[0]->_isUnicodeValid)
        return findRev(QString(QChar(ch)), index);
    else
        FATAL("invalid character cache");

    return -1;
}

int QString::findRev(const char *chs, int index) const
{
    return findRev(QString(chs), index);
}

int QString::findRev( const QString& str, int index, bool cs ) const
{
    // FIXME, use the first character algorithm
    /*
      See QString::find() for explanations.
    */
    int lthis = dataHandle[0]->_length;
    if ( index < 0 )
        index += lthis;

    int lstr = str.dataHandle[0]->_length;
    int delta = lthis - lstr;
    if ( index < 0 || index > lthis || delta < 0 )
        return -1;
    if ( index > delta )
        index = delta;

    const QChar *uthis = unicode();
    const QChar *ustr = str.unicode();
    uint hthis = 0;
    uint hstr = 0;
    int i;
    if ( cs ) {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[index + i].cell();
            hstr += ustr[i].cell();
        }
        i = index;
        while ( true ) {
            if ( hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(QChar)) == 0 )
                return i;
            if ( i == 0 )
                return -1;
            i--;
            hthis -= uthis[i + lstr].cell();
            hthis += uthis[i].cell();
        }
    } else {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[index + i].lower().cell();
            hstr += ustr[i].lower().cell();
        }
        i = index;
        while ( true ) {
            if ( hthis == hstr && equalCaseInsensitive(uthis + i, ustr, lstr) )
                return i;
            if ( i == 0 )
                return -1;
            i--;
            hthis -= uthis[i + lstr].lower().cell();
            hthis += uthis[i].lower().cell();
        }
    }

    // Should never get here.
    return -1;
}


int QString::contains(QChar c, bool cs) const
{
    int count = 0;
    
    KWQStringData *data = *dataHandle;
    
    if (data->_isAsciiValid) {
        if (!IS_ASCII_QCHAR(c))
            return 0;
        const char *cPtr = data->_ascii;
        int n = data->_length;
        char ac = c.unicode();
        if (cs) {                                       // case sensitive
            while (n--)
                count += *cPtr++ == ac;
        } else {                                        // case insensitive
            int lc = tolower(ac);
            while (n--) {
                count += tolower(*cPtr++) == lc;
            }
        }
    } else {
        ASSERT(data->_isUnicodeValid);
        const QChar *uc = data->_unicode;
        int n = data->_length;
        if (cs) {                                       // case sensitive
            while ( n-- )
                count += *uc++ == c;
        } else {                                        // case insensitive
            int lc = tolower(c.unicode());
            while (n--) {
                count += tolower(uc->unicode()) == lc;
                uc++;
            }
        }
    } 

    return count;
}

int QString::contains(char ch) const
{
    return contains(QChar(ch), true);
}

int QString::contains(const char *str, bool caseSensitive) const
{
    if (!str)
        return 0;

    int len = strlen(str);
    char c = *str;

    KWQStringData *data = *dataHandle;
    int n = data->_length;

    n -= len - 1;
    if (n <= 0)
        return 0;

    int count = 0;

    if (data->_isAsciiValid) {
        const char *p = data->_ascii;
        if (caseSensitive) {
            do {
                count += *p == c && memcmp(p + 1, str + 1, len - 1) == 0;
                p++;
            } while (--n);
        } else {
            int lc = tolower(c);
            do {
                count += tolower(*p) == lc && equalCaseInsensitive(p + 1, str + 1, len - 1);
                p++;
            } while (--n);
        }
    } else {
        ASSERT(data->_isUnicodeValid);
        const QChar *p = data->_unicode;
        if (caseSensitive) {
            do {
                count += *p == c && equal(p + 1, str + 1, len - 1);
                p++;
            } while (--n);
        } else {
            int lc = tolower(c);
            do {
                count += tolower(p->unicode()) == lc && equalCaseInsensitive(p + 1, str + 1, len - 1);
                p++;
            } while (--n);
        }
    }

    return count;
}

int QString::contains(const QString &str, bool caseSensitive) const
{
    if (str.isEmpty())
        return 0;

    const QChar *strP = str.unicode();
    int len = str.dataHandle[0]->_length;
    QChar c = *strP;

    const QChar *p = unicode();
    int n = dataHandle[0]->_length;

    n -= len - 1;
    if (n <= 0)
        return 0;

    int count = 0;

    if (caseSensitive) {
        int byteCount = len * sizeof(QChar);
        do {
            count += *p == c && memcmp(p, strP, byteCount) == 0;
            ++p;
        } while (--n);
    } else {
        do {
            count += p->lower() == c && equalCaseInsensitive(p, strP, len) == 0;
            ++p;
        } while (--n);
    }

    return count;
}

bool QString::isAllASCII() const
{
    KWQStringData *data = *dataHandle;

    int n = data->_length;
    if (data->_isAsciiValid) {
        const char *p = data->_ascii;
        while (n--) {
            unsigned char c = *p++;
            if (c > 0x7F) {
                return false;
            }
        }
    } else {
        ASSERT(data->_isUnicodeValid);
        const QChar *p = data->_unicode;
        while (n--) {
            if ((*p++).unicode() > 0x7F) {
                return false;
            }
        }
    }

    return true;
}

bool QString::isAllLatin1() const
{
    KWQStringData *data = *dataHandle;

    if (data->_isAsciiValid) {
        return true;
    }

    ASSERT(data->_isUnicodeValid);
    int n = data->_length;
    const QChar *p = data->_unicode;
    while (n--) {
        if ((*p++).unicode() > 0xFF) {
            return false;
        }
    }

    return true;
}

bool QString::hasFastLatin1() const
{
    KWQStringData *data = *dataHandle;
    return data->_isAsciiValid;
}

void QString::copyLatin1(char *buffer, uint position, uint maxLength) const
{
    KWQStringData *data = *dataHandle;

    int length = data->_length;
    if (position > static_cast<uint>(length))
        length = 0;
    else
        length -= position;
    if (static_cast<uint>(length) > maxLength)
        length = static_cast<int>(maxLength);

    buffer[length] = 0;

    if (data->_isAsciiValid) {
        memcpy(buffer, data->_ascii + position, length);
        return;
    }

    ASSERT(data->_isUnicodeValid);
    const QChar *uc = data->_unicode + position;
    while (length--)
        *buffer++ = *uc++;
}

short QString::toShort(bool *ok, int base) const
{
    int v = toInt(ok, base);
    short sv = v;
    if (sv != v) {
        if (ok)
            *ok = false;
        return 0;
    }
    return sv;
}

unsigned short QString::toUShort(bool *ok, int base) const
{
    uint v = toUInt(ok, base);
    unsigned short sv = v;
    if (sv != v) {
        if (ok)
            *ok = false;
        return 0;
    }
    return sv;
}

int QString::toInt(bool *ok, int base) const
{
    const QChar *p = unicode();
    int val=0;
    int l = dataHandle[0]->_length;
    const int max_mult = INT_MAX / base;
    bool is_ok = false;
    int neg = 0;
    if ( !p )
        goto bye;
    while ( l && p->isSpace() )                 // skip leading space
        l--,p++;
    if ( l && *p == '-' ) {
        l--;
        p++;
        neg = 1;
    } else if ( *p == '+' ) {
        l--;
        p++;
    }

    // NOTE: toUInt() code is similar
    if ( !l || !ok_in_base(*p,base) )
        goto bye;
    while ( l && ok_in_base(*p,base) ) {
        l--;
        int dv;
        int c = p->unicode();
        if ( isdigit(c) ) {
            dv = c - '0';
        } else {
            if ( c >= 'a' )
                dv = c - 'a' + 10;
            else
                dv = c - 'A' + 10;
        }
        if ( val > max_mult || (val == max_mult && dv > (INT_MAX % base)+neg) )
            goto bye;
        val = base*val + dv;
        p++;
    }
    if ( neg )
        val = -val;
    while ( l && p->isSpace() )                 // skip trailing space
        l--,p++;
    if ( !l )
        is_ok = true;
bye:
    if ( ok )
        *ok = is_ok;
    return is_ok ? val : 0;
}

uint QString::toUInt(bool *ok, int base) const
{
    const QChar *p = unicode();
    uint val=0;
    int l = dataHandle[0]->_length;
    const uint max_mult = UINT_MAX / base;
    bool is_ok = false;
    if ( !p )
        goto bye;
    while ( l && p->isSpace() )                 // skip leading space
        l--,p++;
    if ( *p == '+' )
        l--,p++;

    // NOTE: toInt() code is similar
    if ( !l || !ok_in_base(*p,base) )
        goto bye;
    while ( l && ok_in_base(*p,base) ) {
        l--;
        uint dv;
        int c = p->unicode();
        if ( isdigit(c) ) {
            dv = c - '0';
        } else {
            if ( c >= 'a' )
                dv = c - 'a' + 10;
            else
                dv = c - 'A' + 10;
        }
        if ( val > max_mult || (val == max_mult && dv > (UINT_MAX % base)) )
            goto bye;
        val = base*val + dv;
        p++;
    }

    while ( l && p->isSpace() )                 // skip trailing space
        l--,p++;
    if ( !l )
        is_ok = true;
bye:
    if ( ok )
        *ok = is_ok;
    return is_ok ? val : 0;
}

double QString::toDouble(bool *ok) const
{
    if (isEmpty()) {
        if (ok)
            *ok = false;
        return 0;
    }
    const char *s = latin1();
    char *end;
    double val = kjs_strtod(s, &end);
    if (ok)
        *ok = end == 0 || *end == '\0';
    return val;
}

bool QString::findArg(int& pos, int& len) const
{
    char lowest=0;
    for (uint i = 0; i< dataHandle[0]->_length; i++) {
        if ( at(i) == '%' && i + 1 < dataHandle[0]->_length ) {
            char dig = at(i+1);
            if ( dig >= '0' && dig <= '9' ) {
                if ( !lowest || dig < lowest ) {
                    lowest = dig;
                    pos = i;
                    len = 2;
                }
            }
        }
    }
    return lowest != 0;
}

QString QString::arg(const QString &a, int fieldwidth) const
{
    int pos, len;
    QString r = *this;

    if ( !findArg( pos, len ) ) {
        // Make sure the text at least appears SOMEWHERE
        r += ' ';
        pos = r.dataHandle[0]->_length;
        len = 0;
    }

    r.replace( pos, len, a );
    if ( fieldwidth < 0 ) {
        QString s;
        while ( (uint)-fieldwidth > a.dataHandle[0]->_length ) {
            s += ' ';
            fieldwidth++;
        }
        r.insert( pos + a.dataHandle[0]->_length, s );
    } else if ( fieldwidth ) {
        QString s;
        while ( (uint)fieldwidth > a.dataHandle[0]->_length ) {
            s += ' ';
            fieldwidth--;
        }
        r.insert( pos, s );
    }

    return r;
}

QString QString::arg(short replacement, int width) const
{
    return arg(number((int)replacement), width);
}

QString QString::arg(unsigned short replacement, int width) const
{
    return arg(number((uint)replacement), width);
}

QString QString::arg(int replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(uint replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(long replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(unsigned long replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(double replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::left(uint len) const
{ return mid(0, len); }


QString QString::right(uint len) const
{ return mid(length() - len, len); }


QString QString::mid(uint start, uint len) const
{
    if( dataHandle && *dataHandle)
    {
        KWQStringData &data = **dataHandle;
        
        // clip length
        if (start >= data._length)
            return QString();
        
        if (len > data._length - start)
            len = data._length - start;

        if (len == 0)
            return QString();
        
        if (start == 0 && len == data._length)
            return *this;

        ASSERT(start + len >= start &&       // unsigned overflow
               start + len <= data._length); // past the end
        
        // ascii case
        if( data._isAsciiValid && data._ascii )
            return QString( &(data._ascii[start]) , len);
        
        // unicode case
        else if( data._isUnicodeValid && data._unicode )
            return QString( &(data._unicode[start]), len );
    }
    
    // degenerate case
    return QString();
}


QString QString::copy() const
{
    // does not need to be a deep copy
    return QString(*this);
}

QString QString::lower() const
{
    QString s(*this);
    KWQStringData *d = *s.dataHandle;
    int l = d->_length;
    if (l) {
        bool detached = false;
        if (d->_isAsciiValid) {
            char *p = d->_ascii;
            while (l--) {
                char c = *p;
                // FIXME: Doesn't work for 0x80-0xFF.
                if (c >= 'A' && c <= 'Z') {
                    if (!detached) {
                        s.detach();
                        d = *s.dataHandle;
                        p = d->_ascii + d->_length - l - 1;
                        detached = true;
                    }
                    *p = c + ('a' - 'A');
                }
                p++;
            }
        }
        else {
            ASSERT(d->_isUnicodeValid);
            QChar *p = d->_unicode;
            while (l--) {
                QChar c = *p;
                // FIXME: Doesn't work for 0x80-0xFF.
                if (IS_ASCII_QCHAR(c)) {
                    if (c >= 'A' && c <= 'Z') {
                        if (!detached) {
                            s.detach();
                            d = *s.dataHandle;
                            p = d->_unicode + d->_length - l - 1;
                            detached = true;
                        }
                        *p = c + ('a' - 'A');
                    }
                } else {
                    QChar clower = c.lower();
                    if (clower != c) {
                        if (!detached) {
                            s.detach();
                            d = *s.dataHandle;
                            p = d->_unicode + d->_length - l - 1;
                            detached = true;
                        }
                        *p = clower;
                    }
                }
                p++;
            }
        }
    }
    return s;
}

QString QString::stripWhiteSpace() const
{
    if ( isEmpty() )                            // nothing to do
        return *this;
    if ( !at(0).isSpace() && !at(dataHandle[0]->_length-1).isSpace() )
        return *this;

    int start = 0;
    int end = dataHandle[0]->_length - 1;

    QString result = fromLatin1("");
    while ( start<=end && at(start).isSpace() ) // skip white space from start
        start++;
    if ( start > end ) {                        // only white space
        return result;
    }
    while ( end && at(end).isSpace() )          // skip white space from end
        end--;
    int l = end - start + 1;
    
    if (dataHandle[0]->_isAsciiValid){
        result.setLength( l );
        if ( l )
            memcpy(const_cast<char*>(result.dataHandle[0]->ascii()), &ascii()[start], l );
    }
    else if (dataHandle[0]->_isUnicodeValid){
        result.setLength( l );
        if ( l )
            memcpy(result.forceUnicode(), &unicode()[start], sizeof(QChar)*l );
    }
    else
        FATAL("invalid character cache");
    return result;
}

QString QString::simplifyWhiteSpace() const
{
    if ( isEmpty() )                            // nothing to do
        return *this;
    
    QString result;

    if (dataHandle[0]->_isAsciiValid){
        result.setLength( dataHandle[0]->_length );
        const char *from = ascii();
        const char *fromend = from + dataHandle[0]->_length;
        int outc=0;
        
        char *to = const_cast<char*>(result.ascii());
        while ( true ) {
            while ( from!=fromend && QChar(*from).isSpace() )
                from++;
            while ( from!=fromend && !QChar(*from).isSpace() )
                to[outc++] = *from++;
            if ( from!=fromend )
                to[outc++] = ' ';
            else
                break;
        }
        if ( outc > 0 && to[outc-1] == ' ' )
            outc--;
        result.truncate( outc );
    }
    else if (dataHandle[0]->_isUnicodeValid){
        result.setLength( dataHandle[0]->_length );
        const QChar *from = unicode();
        const QChar *fromend = from + dataHandle[0]->_length;
        int outc=0;
        
        QChar *to = result.forceUnicode();
        while ( true ) {
            while ( from!=fromend && from->isSpace() )
                from++;
            while ( from!=fromend && !from->isSpace() )
                to[outc++] = *from++;
            if ( from!=fromend )
                to[outc++] = ' ';
            else
                break;
        }
        if ( outc > 0 && to[outc-1] == ' ' )
            outc--;
        result.truncate( outc );
    }
    else
        FATAL("invalid character cache");
    
    return result;
}

void QString::deref()
{
    dataHandle[0]->deref();
}


QString &QString::setUnicode(const QChar *uni, uint len)
{
    detachAndDiscardCharacters();
    
    // Free our handle if it isn't the shared null handle, and if no-one else is using it.
    bool needToFreeHandle = dataHandle != shared_null_handle && dataHandle[0]->refCount == 1;
        
    if (len == 0) {
        deref();
        if (needToFreeHandle)
            freeHandle(dataHandle);
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else if (len > dataHandle[0]->_maxUnicode || dataHandle[0]->refCount != 1 || !dataHandle[0]->_isUnicodeValid) {
        deref();
        if (needToFreeHandle)
            freeHandle(dataHandle);
        dataHandle = allocateHandle();
        *dataHandle = new KWQStringData(uni, len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        if ( uni )
            memcpy( (void *)unicode(), uni, sizeof(QChar)*len );
        dataHandle[0]->_length = len;
        dataHandle[0]->_isAsciiValid = 0;
    }
    
    return *this;
}


QString &QString::setLatin1(const char *str, int len)
{
    if ( str == 0 )
        return setUnicode(0,0);
    if ( len < 0 )
        len = strlen(str);

    detachAndDiscardCharacters();
    
    // Free our handle if it isn't the shared null handle, and if no-one else is using it.
    bool needToFreeHandle = dataHandle != shared_null_handle && dataHandle[0]->refCount == 1;
   
    if (len+1 > (int)dataHandle[0]->_maxAscii || dataHandle[0]->refCount != 1 || !dataHandle[0]->_isAsciiValid) {
        deref();
        if (needToFreeHandle)
            freeHandle(dataHandle);
        dataHandle = allocateHandle();
        *dataHandle = new KWQStringData(str,len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        strcpy(const_cast<char*>(ascii()), str );
        dataHandle[0]->_length = len;
        dataHandle[0]->_isUnicodeValid = 0;
    }
    return *this;
}

QString &QString::setNum(short n)
{
    return sprintf("%d", n);
}

QString &QString::setNum(unsigned short n)
{
    return sprintf("%u", n);
}

QString &QString::setNum(int n)
{
    return sprintf("%d", n);
}

QString &QString::setNum(uint n)
{
    return sprintf("%u", n);
}

QString &QString::setNum(long n)
{
    return sprintf("%ld", n);
}

QString &QString::setNum(unsigned long n)
{
    return sprintf("%lu", n);
}

QString &QString::setNum(double n)
{
    return sprintf("%.6lg", n);
}

QString &QString::sprintf(const char *format, ...)
{
    va_list args;
    va_start(args, format);
    
    // Do the format once to get the length.
    char ch;
    int result = vsnprintf(&ch, 1, format, args);
    
    // Handle the empty string case to simplify the code below.
    if (result <= 0) { // POSIX returns 0 in error; Windows returns a negative number.
        setUnicode(0, 0);
        return *this;
    }
    unsigned len = result;
    
    // Arrange for storage for the resulting string.
    detachAndDiscardCharacters();
    if (len >= dataHandle[0]->_maxAscii || dataHandle[0]->refCount != 1 || !dataHandle[0]->_isAsciiValid) {
        // Free our handle if it isn't the shared null handle, and if no-one else is using it.
        bool needToFreeHandle = dataHandle != shared_null_handle && dataHandle[0]->refCount == 1;
        deref();
        if (needToFreeHandle)
            freeHandle(dataHandle);
        dataHandle = allocateHandle();
        *dataHandle = new KWQStringData((char *)0, len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        dataHandle[0]->_length = len;
        dataHandle[0]->_isUnicodeValid = 0;
    }

    // Now do the formatting again, guaranteed to fit.
    vsprintf(const_cast<char*>(ascii()), format, args);

    return *this;
}

QString &QString::prepend(const QString &qs)
{
    return insert(0, qs);
}

QString &QString::prepend(const QChar *characters, uint length)
{
    return insert(0, characters, length);
}

QString &QString::append(const QString &qs)
{
    return insert(dataHandle[0]->_length, qs);
}

QString &QString::append(const char *characters, uint length)
{
    return insert(dataHandle[0]->_length, characters, length);
}

QString &QString::append(const QChar *characters, uint length)
{
    return insert(dataHandle[0]->_length, characters, length);
}

QString &QString::insert(uint index, const char *insertChars, uint insertLength)
{
    if (insertLength == 0)
        return *this;
        
    detach();
    
    if (dataHandle[0]->_isAsciiValid){
        uint originalLength = dataHandle[0]->_length;
        char *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        targetChars = const_cast<char*>(ascii());
        
        // Move tail to make space for inserted characters.
        memmove (targetChars+index+insertLength, targetChars+index, originalLength-index);
        
        // Insert characters.
        memcpy (targetChars+index, insertChars, insertLength);
        
        dataHandle[0]->_isUnicodeValid = 0;
    }
    else if (dataHandle[0]->_isUnicodeValid){
        uint originalLength = dataHandle[0]->_length;
        QChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        targetChars = (QChar *)unicode();
        
        // Move tail to make space for inserted characters.
        memmove (targetChars+(index+insertLength), targetChars+index, (originalLength-index)*sizeof(QChar));

        // Insert characters.
        uint i = insertLength;
        QChar *target = targetChars+index;
        
        while (i--)
            *target++ = *insertChars++;        
    }
    else
        FATAL("invalid character cache");
    
    return *this;
}


QString &QString::insert(uint index, const QString &qs)
{
    if (qs.dataHandle[0]->_length == 0)
        return *this;
        
    if (dataHandle[0]->_isAsciiValid && qs.isAllLatin1()) {
        insert(index, qs.latin1(), qs.length());
    }
    else {
        uint insertLength = qs.dataHandle[0]->_length;
        uint originalLength = dataHandle[0]->_length;
        QChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        targetChars = forceUnicode();
        
        // Move tail to make space for inserted characters.
        memmove (targetChars+(index+insertLength), targetChars+index, (originalLength-index)*sizeof(QChar));

        // Insert characters.
        if (qs.dataHandle[0]->_isAsciiValid){
            uint i = insertLength;
            QChar *target = targetChars+index;
            char *a = const_cast<char*>(qs.ascii());
            
            while (i--)
                *target++ = *a++;
        }
        else {
            QChar *insertChars = (QChar *)qs.unicode();
            memcpy (targetChars+index, insertChars, insertLength*sizeof(QChar));
        }
        
        dataHandle[0]->_isAsciiValid = 0;
    }
    
    return *this;
}


QString &QString::insert(uint index, const QChar *insertChars, uint insertLength)
{
    if (insertLength == 0)
        return *this;
        
    forceUnicode();
    
    uint originalLength = dataHandle[0]->_length;
    setLength(originalLength + insertLength);

    QChar *targetChars = const_cast<QChar *>(unicode());
    if (originalLength > index) {
        memmove(targetChars + index + insertLength, targetChars + index, (originalLength - index) * sizeof(QChar));
    }
    memcpy(targetChars + index, insertChars, insertLength * sizeof(QChar));
    
    return *this;
}


QString &QString::insert(uint index, QChar qc)
{
    detach();
    
    if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(qc)){
        uint originalLength = dataHandle[0]->_length;
        char insertChar = (char)qc;
        char *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = const_cast<char*>(ascii());
        
        // Move tail to make space for inserted character.
        memmove (targetChars+index+1, targetChars+index, originalLength-index);
        
        // Insert character.
        targetChars[index] = insertChar;
        targetChars[dataHandle[0]->_length] = 0;

        dataHandle[0]->_isUnicodeValid = 0;
    }
    else {
        uint originalLength = dataHandle[0]->_length;
        QChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = forceUnicode();
        
        // Move tail to make space for inserted character.
        memmove (targetChars+(index+1), targetChars+index, (originalLength-index)*sizeof(QChar));

        targetChars[index] = qc;
    }
    
    return *this;
}


QString &QString::insert(uint index, char ch)
{
    detach();
    
    if (dataHandle[0]->_isAsciiValid) {
        uint originalLength = dataHandle[0]->_length;
        char *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = const_cast<char*>(ascii());
        
        // Move tail to make space for inserted character.
        memmove (targetChars+index+1, targetChars+index, originalLength-index);
        
        // Insert character.
        targetChars[index] = ch;
        targetChars[dataHandle[0]->_length] = 0;

        dataHandle[0]->_isUnicodeValid = 0;
    }
    else if (dataHandle[0]->_isUnicodeValid){
        uint originalLength = dataHandle[0]->_length;
        QChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = (QChar *)unicode();
        
        // Move tail to make space for inserted character.
        memmove (targetChars+(index+1), targetChars+index, (originalLength-index)*sizeof(QChar));

        targetChars[index] = (QChar)ch;
    }
    else
        FATAL("invalid character cache");
    
    return *this;
}

// Copy KWQStringData if necessary. Must be called before the string data is mutated.
void QString::detach()
{
    KWQStringData *oldData = *dataHandle;

    if (oldData->refCount == 1 && oldData != shared_null)
        return;

    // Copy data for this string so we can safely mutate it.
    KWQStringData *newData;
    if (oldData->_isAsciiValid)
        newData = new KWQStringData(oldData->ascii(), oldData->_length);
    else
        newData = new KWQStringData(oldData->unicode(), oldData->_length);
    newData->_isHeapAllocated = 1;

    // There is now one less client for the old data.
    oldData->deref();

    // If the old data is our internal data, then we'll keep that.
    // This decreases the chance we'll have to do a detachInternal later
    // when this object is destroyed.
    if (oldData == &internalData) {
        newData->refCount = oldData->refCount;
        oldData->refCount = 1;
        *dataHandle = newData;
        newData = oldData;
    }

    // Create a new handle.
    dataHandle = allocateHandle();
    *dataHandle = newData;
}

void QString::detachAndDiscardCharacters()
{
    // Missing optimization: Don't bother copying the old data if we detach.
    detach();
}

QString &QString::remove(uint index, uint len)
{
    uint olen = dataHandle[0]->_length;
    if ( index >= olen  ) {
        // range problems
    } else if ( index + len >= olen ) {  // index ok
        setLength( index );
    } else if ( len != 0 ) {
        // Missing optimization: Could avoid copying characters we are going to remove
        // by making a special version of detach().

        detach();
        
        if (dataHandle[0]->_isAsciiValid){
            memmove( dataHandle[0]->ascii()+index, dataHandle[0]->ascii()+index+len,
                    sizeof(char)*(olen-index-len) );
            setLength( olen-len );
            dataHandle[0]->_isUnicodeValid = 0;
        }
        else if (dataHandle[0]->_isUnicodeValid){
            memmove( dataHandle[0]->unicode()+index, dataHandle[0]->unicode()+index+len,
                    sizeof(QChar)*(olen-index-len) );
            setLength( olen-len );
        }
        else
            FATAL("invalid character cache");
    }
    return *this;
}

QString &QString::replace( uint index, uint len, const QString &str )
{
    return remove(index, len).insert(index, str);
}

QString &QString::replace(char pattern, const QString &str)
{
    int slen = str.dataHandle[0]->_length;
    int index = 0;
    while ((index = find(pattern, index)) >= 0) {
        replace(index, 1, str);
        index += slen;
    }
    return *this;
}


QString &QString::replace(QChar pattern, const QString &str)
{
    int slen = str.dataHandle[0]->_length;
    int index = 0;
    while ((index = find(pattern, index)) >= 0) {
        replace(index, 1, str);
        index += slen;
    }
    return *this;
}


QString &QString::replace(const QString &pattern, const QString &str)
{
    if (pattern.isEmpty())
        return *this;
    int plen = pattern.dataHandle[0]->_length;
    int slen = str.dataHandle[0]->_length;
    int index = 0;
    while ((index = find(pattern, index)) >= 0) {
        replace(index, plen, str);
        index += slen;
    }
    return *this;
}


QString &QString::replace(const QRegExp &qre, const QString &str)
{
    if ( isEmpty() )
        return *this;
    int index = 0;
    int slen  = str.dataHandle[0]->_length;
    int len;
    while ( index < (int)dataHandle[0]->_length ) {
        index = qre.match( *this, index, &len);
        if ( index >= 0 ) {
            replace( index, len, str );
            index += slen;
            if ( !len )
                break;  // Avoid infinite loop on 0-length matches, e.g. [a-z]*
        }
        else
            break;
    }
    return *this;
}


QString &QString::replace(QChar oldChar, QChar newChar)
{
    if (oldChar != newChar && find(oldChar) != -1) {
        unsigned length = dataHandle[0]->_length;
        
        detach();
        if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(newChar)) {
            char *p = const_cast<char *>(ascii());
            dataHandle[0]->_isUnicodeValid = 0;
            char oldC = oldChar;
            char newC = newChar;
            for (unsigned i = 0; i != length; ++i) {
                if (p[i] == oldC) {
                    p[i] = newC;
                }
            }
        } else {
            QChar *p = const_cast<QChar *>(unicode());
            dataHandle[0]->_isAsciiValid = 0;
            for (unsigned i = 0; i != length; ++i) {
                if (p[i] == oldChar) {
                    p[i] = newChar;
                }
            }
        }
    }
    
    return *this;
}


QChar *QString::forceUnicode()
{
    detach();
    QChar *result = const_cast<QChar *>(unicode());
    dataHandle[0]->_isAsciiValid = 0;
    return result;
}


// Increase buffer size if necessary.  Newly allocated
// bytes will contain garbage.
void QString::setLength(uint newLen)
{
    if (newLen == 0) {
        setUnicode(0, 0);
        return;
    }

    // Missing optimization: Could avoid copying characters we are going to remove
    // by making a special version of detach().
    detach();

    ASSERT(dataHandle != shared_null_handle);
    
    if (dataHandle[0]->_isAsciiValid){
        if (newLen+1 > dataHandle[0]->_maxAscii) {
            dataHandle[0]->increaseAsciiSize(newLen+1);
        }
        // Ensure null termination, although newly allocated
        // bytes contain garbage.
        dataHandle[0]->_ascii[newLen] = 0;
    }
    else if (dataHandle[0]->_isUnicodeValid){
        if (newLen > dataHandle[0]->_maxUnicode) {
            dataHandle[0]->increaseUnicodeSize(newLen);
        }
    }
    else
        FATAL("invalid character cache");

    dataHandle[0]->_length = newLen;
}


void QString::truncate(uint newLen)
{
    if ( newLen < dataHandle[0]->_length )
        setLength( newLen );
}

void QString::fill(QChar qc, int len)
{
    detachAndDiscardCharacters();
    
    // len == -1 means fill to string length.
    if (len < 0) {
        len = dataHandle[0]->_length;
    }
        
    if (len == 0) {
        if (dataHandle != shared_null_handle) {
            ASSERT(dataHandle[0]->refCount == 1);
            deref();
            freeHandle(dataHandle);
            dataHandle = makeSharedNullHandle();
            shared_null->ref();
        }
    } else {
        if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(qc)) {
            setLength(len);
            char *nd = const_cast<char*>(ascii());
            while (len--) 
                *nd++ = (char)qc;
            dataHandle[0]->_isUnicodeValid = 0;
        } else {
            setLength(len);
            QChar *nd = forceUnicode();
            while (len--) 
                *nd++ = qc;
        }
    }
}

QString &QString::append(QChar qc)
{
    detach();
    
    KWQStringData *thisData = *dataHandle;
    if (thisData->_isUnicodeValid && thisData->_length + 1 < thisData->_maxUnicode){
        thisData->_unicode[thisData->_length] = qc;
        thisData->_length++;
        thisData->_isAsciiValid = 0;
        return *this;
    }
    else if (thisData->_isAsciiValid && IS_ASCII_QCHAR(qc) && thisData->_length + 2 < thisData->_maxAscii){
        thisData->_ascii[thisData->_length] = (char)qc;
        thisData->_length++;
        thisData->_ascii[thisData->_length] = 0;
        thisData->_isUnicodeValid = 0;
        return *this;
    }
    return insert(thisData->_length, qc);
}

QString &QString::append(char ch)
{
    detach();
    
    KWQStringData *thisData = *dataHandle;
    if (thisData->_isUnicodeValid && thisData->_length + 1 < thisData->_maxUnicode){
        thisData->_unicode[thisData->_length] = (QChar)ch;
        thisData->_length++;
        thisData->_isAsciiValid = 0;
        return *this;
    }
    else if (thisData->_isAsciiValid && thisData->_length + 2 < thisData->_maxAscii){
        thisData->_ascii[thisData->_length] = ch;
        thisData->_length++;
        thisData->_ascii[thisData->_length] = 0;
        thisData->_isUnicodeValid = 0;
        return *this;
    }
    return insert(thisData->_length, ch);
}

void QString::reserve(uint length)
{
    if (length > dataHandle[0]->_maxUnicode) {
        detach();
        dataHandle[0]->increaseUnicodeSize(length);
    }
}

bool operator==(const QString &s1, const QString &s2)
{
    if (s1.dataHandle[0]->_isAsciiValid && s2.dataHandle[0]->_isAsciiValid) {
        return strcmp(s1.ascii(), s2.ascii()) == 0;
    }
    return s1.dataHandle[0]->_length == s2.dataHandle[0]->_length
        && memcmp(s1.unicode(), s2.unicode(), s1.dataHandle[0]->_length * sizeof(QChar)) == 0;
}

bool operator==(const QString &s1, const char *chs)
{
    if (!chs)
        return s1.isNull();
    KWQStringData *d = s1.dataHandle[0];
    uint len = d->_length;
    if (d->_isAsciiValid) {
        const char *s = s1.ascii();
        for (uint i = 0; i != len; ++i) {
            char c = chs[i];
            if (!c || s[i] != c)
                return false;
        }
    } else {
        const QChar *s = s1.unicode();
        for (uint i = 0; i != len; ++i) {
            char c = chs[i];
            if (!c || s[i] != c)
                return false;
        }
    }
    return chs[len] == '\0';
}

QString operator+(const QString &qs1, const QString &qs2)
{
    return QString(qs1) += qs2;
}

QString operator+(const QString &qs, const char *chs)
{
    return QString(qs) += chs;
}

QString operator+(const QString &qs, QChar qc)
{
    return QString(qs) += qc;
}

QString operator+(const QString &qs, char ch)
{
    return QString(qs) += ch;
}

QString operator+(const char *chs, const QString &qs)
{
    return QString(chs) += qs;
}

QString operator+(QChar qc, const QString &qs)
{
    return QString(qc) += qs;
}

QString operator+(char ch, const QString &qs)
{
    return QString(QChar(ch)) += qs;
}

QConstString::QConstString(const QChar* unicode, uint length) :
    QString(new KWQStringData((QChar *)unicode, length, length), true)
{
}

QConstString::~QConstString()
{
    KWQStringData *data = *dataHandle;
    if (data->refCount > 1) {
        QChar *tp;
        if (data->_length <= QS_INTERNAL_BUFFER_UCHARS) {
            data->_maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
            tp = (QChar *)&data->_internalBuffer[0];
        } else {
            data->_maxUnicode = ALLOC_QCHAR_GOOD_SIZE(data->_length);
            tp = ALLOC_QCHAR(data->_maxUnicode);
        }
        memcpy(tp, data->_unicode, data->_length * sizeof(QChar));
        data->_unicode = tp;
        data->_isUnicodeValid = 1;
        data->_isAsciiValid = 0;
    } else {
        data->_unicode = 0;
    }
}

struct HandlePageNode
{
    HandlePageNode *next;
    HandlePageNode *previous;
    void *nodes;
};

struct HandleNode {
    union {
        struct {
            unsigned short next;
            unsigned short previous;
        } internalNode;
        
        HandleNode *freeNodes;  // Always at block[0] in page.
        
        HandlePageNode *pageNode;   // Always at block[1] in page
        
        void *handle;
    } type;
};

static const size_t pageSize = 4096;
static const uintptr_t pageMask = ~(pageSize - 1);
static const size_t nodeBlockSize = pageSize / sizeof(HandleNode);

#define TO_NODE_OFFSET(ptr)   ((uint)(((uint)ptr - (uint)base)/sizeof(HandleNode)))
#define TO_NODE_ADDRESS(offset,base) ((HandleNode *)(offset*sizeof(HandleNode) + (uint)base))

static HandleNode *initializeHandleNodeBlock(HandlePageNode *pageNode)
{
    uint i;
    HandleNode* block;
    HandleNode* aNode;

#ifdef WIN32
    block = (HandleNode*)VirtualAlloc(0, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#else
    block = (HandleNode*)valloc(pageSize);
#endif

    for (i = 2; i < nodeBlockSize; i++) {
        aNode = &block[i];
        if (i > 2)
            aNode->type.internalNode.previous = i-1;
        else
            aNode->type.internalNode.previous = 0;
        if (i != nodeBlockSize - 1)
            aNode->type.internalNode.next = i+1;
        else
            aNode->type.internalNode.next = 0;
    }
    block[0].type.freeNodes = &block[nodeBlockSize - 1];
    block[1].type.pageNode = pageNode;

    return block;
}

static HandlePageNode *allocatePageNode()
{
    HandlePageNode *node = (HandlePageNode *)fastMalloc(sizeof(HandlePageNode));
    node->next = node->previous = 0;
    node->nodes = initializeHandleNodeBlock(node);
    return node;
}

static HandleNode *allocateNode(HandlePageNode *pageNode)
{
    HandleNode *block = (HandleNode *)pageNode->nodes;
    HandleNode *freeNodes = block[0].type.freeNodes;
    HandleNode *allocated;
    
    // Check to see if we're out of nodes.
    if (freeNodes == 0) {
        FATAL("out of nodes");
        return 0;
    }
    
    // Remove node from end of free list 
    allocated = freeNodes;
    if (allocated->type.internalNode.previous >= 2) {
        block[0].type.freeNodes = TO_NODE_ADDRESS(allocated->type.internalNode.previous, block);
        block[0].type.freeNodes->type.internalNode.next = 0;
    }
    else {
        // Used last node on this page.
        block[0].type.freeNodes = 0;
        
        freeNodeAllocationPages = freeNodeAllocationPages->previous;
        if (freeNodeAllocationPages)
            freeNodeAllocationPages->next = 0;

        pageNode->previous = usedNodeAllocationPages;
        pageNode->next = 0;
        if (usedNodeAllocationPages)
            usedNodeAllocationPages->next = pageNode;
        usedNodeAllocationPages = pageNode;        
    }

    return allocated;
}

void freeHandle(KWQStringData **_free)
{
#if CHECK_FOR_HANDLE_LEAKS
    fastFree(_free);
    return;
#endif

    HandleNode *free = (HandleNode *)_free;
    HandleNode *base = (HandleNode *)((uintptr_t)free & pageMask);
    HandleNode *freeNodes = base[0].type.freeNodes;
    HandlePageNode *pageNode = base[1].type.pageNode;
    
    if (freeNodes == 0){
        free->type.internalNode.previous = 0;
    }
    else {
        // Insert at head of free list.
        free->type.internalNode.previous = TO_NODE_OFFSET(freeNodes);
        freeNodes->type.internalNode.next = TO_NODE_OFFSET(free);
    }
    free->type.internalNode.next = 0;
    base[0].type.freeNodes = free;
    
    // Remove page from used/free list and place on free list
    if (freeNodeAllocationPages != pageNode) {
        if (pageNode->previous)
            pageNode->previous->next = pageNode->next;
        if (pageNode->next)
            pageNode->next->previous = pageNode->previous;
        if (usedNodeAllocationPages == pageNode)
            usedNodeAllocationPages = pageNode->previous;
    
        pageNode->previous = freeNodeAllocationPages;
        pageNode->next = 0;
        if (freeNodeAllocationPages)
            freeNodeAllocationPages->next = pageNode;
        freeNodeAllocationPages = pageNode;
    }
}

QString QString::fromUtf8(const char *chs)
{
    return WebCore::TextEncoding(WebCore::UTF8Encoding).toUnicode(chs, strlen(chs));
}

QString QString::fromUtf8(const char *chs, int len)
{
    return WebCore::TextEncoding(WebCore::UTF8Encoding).toUnicode(chs, len);
}

QCString QString::utf8(int& length) const
{
    QCString result = WebCore::TextEncoding(UTF8Encoding).fromUnicode(*this);
    length = result.length();
    return result;
}

