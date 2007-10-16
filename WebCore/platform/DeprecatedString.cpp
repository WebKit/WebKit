/*
 * Copyright (C) 2004, 2005, 2006, 2007 Apple Inc. All rights reserved.
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
#include "DeprecatedString.h"

#include "CString.h"
#include "FloatConversion.h"
#include "Logging.h"
#include "PlatformString.h"
#include "RegularExpression.h"
#include "TextEncoding.h"
#include <kjs/dtoa.h>
#include <kjs/identifier.h>
#include <stdarg.h>
#include <stdio.h>
#include <wtf/Platform.h>
#include <wtf/StringExtras.h>

#if PLATFORM(WIN_OS)
#include <windows.h>
#endif

#if PLATFORM(QT)
#include <QString>
#endif

using namespace std;
using namespace KJS;
using namespace WTF;

namespace WebCore {

COMPILE_ASSERT(sizeof(DeprecatedChar) == 2, deprecated_char_is_2_bytes)

#define CHECK_FOR_HANDLE_LEAKS 0

#if PLATFORM(SYMBIAN)
#undef CHECK_FOR_HANDLE_LEAKS
// symbian:fixme need page aligned allocations as Symbian platform does not have support for valloc
#define CHECK_FOR_HANDLE_LEAKS 1
#endif

#define ALLOC_QCHAR_GOOD_SIZE(X) (X)
#define ALLOC_CHAR_GOOD_SIZE(X) (X)

#define ALLOC_CHAR(N) (char*)fastMalloc(N)
#define REALLOC_CHAR(P, N) (char *)fastRealloc(P, N)
#define DELETE_CHAR(P) fastFree(P)

#define WEBCORE_ALLOCATE_CHARACTERS(N) (DeprecatedChar*)fastMalloc(sizeof(DeprecatedChar)*(N))
#define WEBCORE_REALLOCATE_CHARACTERS(P, N) (DeprecatedChar *)fastRealloc(P, sizeof(DeprecatedChar)*(N))
#define DELETE_QCHAR(P) fastFree(P)

#ifndef CHECK_FOR_HANDLE_LEAKS
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
#endif

static inline DeprecatedStringData **allocateHandle()
{
#ifdef CHECK_FOR_HANDLE_LEAKS
    return static_cast<DeprecatedStringData **>(fastMalloc(sizeof(DeprecatedStringData *)));
#else

    initializeHandleNodes();
    
    return reinterpret_cast<DeprecatedStringData **>(allocateNode(freeNodeAllocationPages));
#endif
}

static void freeHandle(DeprecatedStringData **);

#define IS_ASCII_QCHAR(c) ((c).unicode() > 0 && (c).unicode() <= 0xff)

static const int caseDelta = ('a' - 'A');

const char * const DeprecatedString::null = 0;

DeprecatedStringData *DeprecatedString::shared_null = 0;
DeprecatedStringData **DeprecatedString::shared_null_handle = 0;

// -------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------

static inline int ucstrcmp( const DeprecatedString &as, const DeprecatedString &bs )
{
    const DeprecatedChar *a = as.unicode();
    const DeprecatedChar *b = bs.unicode();
    if ( a == b )
        return 0;
    if ( a == 0 )
        return 1;
    if ( b == 0 )
        return -1;
    int l = min(as.length(), bs.length());
    while ( l-- && *a == *b )
        a++,b++;
    if ( l == -1 )
        return ( as.length() - bs.length() );
    return a->unicode() - b->unicode();
}

static bool equal(const DeprecatedChar *a, const char *b, int l)
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
        if (toASCIILower(*a) != toASCIILower(*b))
            return false;
        a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const DeprecatedChar *a, const char *b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (toASCIILower(a->unicode()) != static_cast<unsigned char>(toASCIILower(*b)))
            return false;
        a++; b++;
    }
    return true;
}

static bool equalCaseInsensitive(const DeprecatedChar *a, const DeprecatedChar *b, int l)
{
    ASSERT(l >= 0);
    while (l--) {
        if (toASCIILower(a->unicode()) != toASCIILower(b->unicode()))
            return false;
        a++; b++;
    }
    return true;
}

static inline bool equalCaseInsensitive(char c1, char c2)
{
    return toASCIILower(c1) == toASCIILower(c2);
}

static inline bool equalCaseInsensitive(DeprecatedChar c1, char c2)
{
    return toASCIILower(c1.unicode()) == static_cast<unsigned char>(toASCIILower(c2));
}

static bool isCharacterAllowedInBase(DeprecatedChar c, int base)
{
    ::UChar uc = c.unicode();
    if (uc > 0x7F)
        return false;
    if (isASCIIDigit(uc))
        return uc - '0' < base;
    if (isASCIIAlpha(uc)) {
        if (base > 36)
            base = 36;
        return (uc >= 'a' && uc < 'a' + base - 10)
            || (uc >= 'A' && uc < 'A' + base - 10);
    }
    return false;
}

// -------------------------------------------------------------------------
// DeprecatedStringData
// -------------------------------------------------------------------------

// FIXME, make constructor explicity take a 'copy' flag.
// This can be used to hand off ownership of allocated data when detaching and
// deleting QStrings.

DeprecatedStringData::DeprecatedStringData() :
        refCount(1), _length(0), _unicode(0), _ascii(0), _maxUnicode(WEBCORE_DS_INTERNAL_BUFFER_UCHARS), _isUnicodeValid(0), _isHeapAllocated(0), _maxAscii(WEBCORE_DS_INTERNAL_BUFFER_CHARS), _isAsciiValid(1) 
{ 
    _ascii = _internalBuffer;
    _internalBuffer[0] = 0;
}

void DeprecatedStringData::initialize()
{
    refCount = 1;
    _length = 0;
    _unicode = 0;
    _ascii = _internalBuffer;
    _maxUnicode = WEBCORE_DS_INTERNAL_BUFFER_UCHARS;
    _isUnicodeValid = 0;
    _maxAscii = WEBCORE_DS_INTERNAL_BUFFER_CHARS;
    _isAsciiValid = 1;
    _internalBuffer[0] = 0;
    _isHeapAllocated = 0;
}

// Don't copy data.
DeprecatedStringData::DeprecatedStringData(DeprecatedChar *u, unsigned l, unsigned m) :
        refCount(1), _length(l), _unicode(u), _ascii(0), _maxUnicode(m), _isUnicodeValid(1), _isHeapAllocated(0), _maxAscii(WEBCORE_DS_INTERNAL_BUFFER_CHARS), _isAsciiValid(0)
{
    ASSERT(m >= l);
}

// Don't copy data.
void DeprecatedStringData::initialize(DeprecatedChar *u, unsigned l, unsigned m)
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
DeprecatedStringData::DeprecatedStringData(const DeprecatedChar *u, unsigned l)
{
    initialize (u, l);
}

// Copy data
void DeprecatedStringData::initialize(const DeprecatedChar *u, unsigned l)
{
    refCount = 1;
    _length = l;
    _ascii = 0;
    _isUnicodeValid = 1;
    _maxAscii = 0;
    _isAsciiValid = 0;
    _isHeapAllocated = 0;

    if (l > WEBCORE_DS_INTERNAL_BUFFER_UCHARS) {
        _maxUnicode = ALLOC_QCHAR_GOOD_SIZE(l);
        _unicode = WEBCORE_ALLOCATE_CHARACTERS(_maxUnicode);
        memcpy(_unicode, u, l*sizeof(DeprecatedChar));
    } else {
        _maxUnicode = WEBCORE_DS_INTERNAL_BUFFER_UCHARS;
        _unicode = (DeprecatedChar *)_internalBuffer;
        if (l)
            memcpy(_internalBuffer, u, l*sizeof(DeprecatedChar));
    }
}


// Copy data
DeprecatedStringData::DeprecatedStringData(const char *a, unsigned l)
{
    initialize(a, l);
}


// Copy data
void DeprecatedStringData::initialize(const char *a, unsigned l)
{
    refCount = 1;
    _length = l;
    _unicode = 0;
    _isUnicodeValid = 0;
    _maxUnicode = 0;
    _isAsciiValid = 1;
    _isHeapAllocated = 0;

    if (l > WEBCORE_DS_INTERNAL_BUFFER_CHARS) {
        _maxAscii = ALLOC_CHAR_GOOD_SIZE(l+1);
        _ascii = ALLOC_CHAR(_maxAscii);
        if (a)
            memcpy(_ascii, a, l);
        _ascii[l] = 0;
    } else {
        _maxAscii = WEBCORE_DS_INTERNAL_BUFFER_CHARS;
        _ascii = _internalBuffer;
        if (a)
            memcpy(_internalBuffer, a, l);
        _internalBuffer[l] = 0;
    }
}

DeprecatedStringData* DeprecatedStringData::createAndAdopt(DeprecatedStringData &o)
{
    DeprecatedStringData* data = new DeprecatedStringData();
    data->adopt(o);
    return data;
}

void DeprecatedStringData::adopt(DeprecatedStringData& o)
{
    ASSERT(refCount == 1);
    _length = o._length;
    _unicode = o._unicode;
    _ascii = o._ascii;
    _maxUnicode = o._maxUnicode;
    _isUnicodeValid = o._isUnicodeValid;
    _isHeapAllocated = 0;
    _maxAscii = o._maxAscii;
    _isAsciiValid = o._isAsciiValid;

    // Handle the case where either the Unicode or 8-bit pointer was
    // pointing to the internal buffer. We need to point at the
    // internal buffer in the new object, and copy the characters.
    if (_unicode == reinterpret_cast<DeprecatedChar *>(o._internalBuffer)) {
        if (_isUnicodeValid) {
            ASSERT(!_isAsciiValid || _ascii != o._internalBuffer);
            ASSERT(_length <= WEBCORE_DS_INTERNAL_BUFFER_UCHARS);
            memcpy(_internalBuffer, o._internalBuffer, _length * sizeof(DeprecatedChar));
            _unicode = reinterpret_cast<DeprecatedChar *>(_internalBuffer);
        } else {
            _unicode = 0;
        }
    }
    if (_ascii == o._internalBuffer) {
        if (_isAsciiValid) {
            ASSERT(_length <= WEBCORE_DS_INTERNAL_BUFFER_CHARS);
            memcpy(_internalBuffer, o._internalBuffer, _length);
            _internalBuffer[_length] = 0;
            _ascii = _internalBuffer;
        } else {
            _ascii = 0;
        }
    }

    // Clean up the other DeprecatedStringData just enough so that it can be destroyed
    // cleanly. It's not in a good enough state to use, but that's OK. It just
    // needs to be in a state where ~DeprecatedStringData won't do anything harmful,
    // and setting these to 0 will do that (preventing any double-free problems).
    o._unicode = 0;
    o._ascii = 0;
}

DeprecatedStringData *DeprecatedString::makeSharedNull()
{
    if (!shared_null) {
        shared_null = new DeprecatedStringData;
        shared_null->ref();
        shared_null->_maxAscii = 0;
        shared_null->_maxUnicode = 0;
        shared_null->_unicode = (DeprecatedChar *)&shared_null->_internalBuffer[0]; 
        shared_null->_isUnicodeValid = 1;   
    }
    return shared_null;
}

DeprecatedStringData **DeprecatedString::makeSharedNullHandle()
{
    if (!shared_null_handle) {
        shared_null_handle = allocateHandle();
        *shared_null_handle = makeSharedNull();
    }
    return shared_null_handle;
}

DeprecatedStringData::~DeprecatedStringData()
{
    ASSERT(refCount == 0);
    if (_unicode && !isUnicodeInternal())
        DELETE_QCHAR(_unicode);
    if (_ascii && !isAsciiInternal())
        DELETE_CHAR(_ascii);
}

void DeprecatedStringData::increaseAsciiSize(unsigned size)
{
    ASSERT(this != DeprecatedString::shared_null);
        
    unsigned newSize = (unsigned)ALLOC_CHAR_GOOD_SIZE((size * 3 + 1) / 2);
    
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


void DeprecatedStringData::increaseUnicodeSize(unsigned size)
{
    ASSERT(size > _length);
    ASSERT(this != DeprecatedString::shared_null);
        
    unsigned newSize = (unsigned)ALLOC_QCHAR_GOOD_SIZE((size * 3 + 1) / 2);
    
    if (!_isUnicodeValid)
        makeUnicode();
    ASSERT(_isUnicodeValid);

    if (isUnicodeInternal()) {
        DeprecatedChar *newUni = WEBCORE_ALLOCATE_CHARACTERS(newSize);
        if (_length)
            memcpy(newUni, _unicode, _length*sizeof(DeprecatedChar));
        _unicode = newUni;
    } else {
        _unicode = WEBCORE_REALLOCATE_CHARACTERS(_unicode, newSize);
    }
    
    _maxUnicode = newSize;
    _isUnicodeValid = 1;
    _isAsciiValid = 0;
}


char *DeprecatedStringData::makeAscii()
{
    ASSERT(this != DeprecatedString::shared_null);
        
    if (_isUnicodeValid){
        DeprecatedChar copyBuf[WEBCORE_DS_INTERNAL_BUFFER_CHARS];
        DeprecatedChar *str;
        
        if (_ascii && !isAsciiInternal())
            DELETE_QCHAR(_ascii);
            
        if (_length < WEBCORE_DS_INTERNAL_BUFFER_CHARS){
            if (isUnicodeInternal()) {
                unsigned i = _length;
                DeprecatedChar *tp = &copyBuf[0], *fp = _unicode;
                while (i--)
                    *tp++ = *fp++;
                str = &copyBuf[0];
                _isUnicodeValid = 0;
            }
            else
                str = _unicode;
            _ascii = _internalBuffer;
            _maxAscii = WEBCORE_DS_INTERNAL_BUFFER_CHARS;
        }
        else {
            unsigned newSize = ALLOC_CHAR_GOOD_SIZE(_length+1);
            _ascii = ALLOC_CHAR(newSize);
            _maxAscii = newSize;
            str = _unicode;
        }

        unsigned i = _length;
        char* cp = _ascii;
        while (i--)
            // FIXME: this converts non-Latin1 characters to '\0', which may be not what we want in some cases.
            // In particular, toDouble() may fail to report errors, believing that the string ends earlier
            // than it actually does.
            *cp++ = (*str++).latin1();
        *cp = 0;
        
        _isAsciiValid = 1;
    }
    else if (!_isAsciiValid)
        FATAL("ASCII character cache not valid");
        
    return _ascii;
}


DeprecatedChar *DeprecatedStringData::makeUnicode()
{
    ASSERT(this != DeprecatedString::shared_null);
        
    if (_isAsciiValid){
        char copyBuf[WEBCORE_DS_INTERNAL_BUFFER_CHARS];
        char *str;
        
        if (_unicode && !isUnicodeInternal())
            DELETE_QCHAR(_unicode);
            
        if (_length <= WEBCORE_DS_INTERNAL_BUFFER_UCHARS){
            if (isAsciiInternal()) {
                unsigned i = _length;
                char *tp = &copyBuf[0], *fp = _ascii;
                while (i--)
                    *tp++ = *fp++;
                str = &copyBuf[0];
                _isAsciiValid = 0;
            }
            else
                str = _ascii;
            _unicode = (DeprecatedChar *)_internalBuffer;
            _maxUnicode = WEBCORE_DS_INTERNAL_BUFFER_UCHARS;
        }
        else {
            unsigned newSize = ALLOC_QCHAR_GOOD_SIZE(_length);
            _unicode = WEBCORE_ALLOCATE_CHARACTERS(newSize);
            _maxUnicode = newSize;
            str = _ascii;
        }
        unsigned i = _length;
        DeprecatedChar *cp = _unicode;
        while ( i-- )
            *cp++ = *str++;
        
        _isUnicodeValid = 1;
    }
    else if (!_isUnicodeValid)
        FATAL("invalid character cache");

    return _unicode;
}


// -------------------------------------------------------------------------
// DeprecatedString
// -------------------------------------------------------------------------


DeprecatedString DeprecatedString::number(int n)
{
    DeprecatedString qs;
    qs.setNum(n);
    return qs;
}

DeprecatedString DeprecatedString::number(unsigned n)
{
    DeprecatedString qs;
    qs.setNum(n);
    return qs;
}

DeprecatedString DeprecatedString::number(long n)
{
    DeprecatedString qs;
    qs.setNum(n);
    return qs;
}

DeprecatedString DeprecatedString::number(unsigned long n)
{
    DeprecatedString qs;
    qs.setNum(n);
    return qs;
}

DeprecatedString DeprecatedString::number(double n)
{
    DeprecatedString qs;
    qs.setNum(n);
    return qs;
}

inline void DeprecatedString::detachIfInternal()
{
    DeprecatedStringData *oldData = *dataHandle;
    if (oldData->refCount > 1 && oldData == &internalData) {
        DeprecatedStringData *newData = DeprecatedStringData::createAndAdopt(*oldData);
        newData->_isHeapAllocated = 1;
        newData->refCount = oldData->refCount;
        oldData->refCount = 1;
        oldData->deref();
        *dataHandle = newData;    
    }
}

const DeprecatedChar *DeprecatedString::stableUnicode()
{
    // if we're using the internal data of another string, detach now
    if (!dataHandle[0]->_isHeapAllocated && *dataHandle != &internalData) {
        detach();
    }
    return unicode();
}


DeprecatedString::~DeprecatedString()
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
    // if *dataHandle points to our internal DeprecatedStringData. If we just detached,
    // this will remove the extra ref from the new handle.
    dataHandle[0]->deref();

    ASSERT(*dataHandle != &internalData || dataHandle[0]->refCount == 0);
    
    if (needToFreeHandle)
        freeHandle(dataHandle);

#ifndef NDEBUG
    dataHandle = 0;
#endif
}


DeprecatedString::DeprecatedString()
{
    internalData.deref();
    dataHandle = makeSharedNullHandle();
    dataHandle[0]->ref();
}


// Careful, just used by DeprecatedConstString
DeprecatedString::DeprecatedString(DeprecatedStringData *constData, bool /*dummy*/) 
{
    internalData.deref();
    dataHandle = allocateHandle();
    *dataHandle = constData;
    
    // The DeprecatedConstString constructor allocated the DeprecatedStringData.
    constData->_isHeapAllocated = 1;
}


DeprecatedString::DeprecatedString(DeprecatedChar qc)
{
    dataHandle = allocateHandle();

    // Copy the DeprecatedChar.
    if (IS_ASCII_QCHAR(qc)) {
        char c = qc.unicode(); 
        *dataHandle = &internalData;
        internalData.initialize( &c, 1 );
    }
    else {
        *dataHandle = &internalData;
        internalData.initialize( &qc, 1 );
    }
}

DeprecatedString::DeprecatedString(const DeprecatedChar *unicode, unsigned length)
{
    if (!unicode || !length) {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else {
        dataHandle = allocateHandle();

        // Copy the DeprecatedChar *
        *dataHandle = &internalData;
        internalData.initialize(unicode, length);
    }
}

DeprecatedString::DeprecatedString(const char *chs)
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

DeprecatedString::DeprecatedString(const char *chs, int len)
{
    dataHandle = allocateHandle();
    *dataHandle = &internalData;
    internalData.initialize(chs,len);
}

DeprecatedString::DeprecatedString(const DeprecatedString &qs) : dataHandle(qs.dataHandle)
{
    internalData.deref();
    dataHandle[0]->ref();
}

DeprecatedString &DeprecatedString::operator=(const DeprecatedString &qs)
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

DeprecatedString &DeprecatedString::operator=(const DeprecatedCString &qcs)
{
    return setLatin1(qcs);
}

DeprecatedString &DeprecatedString::operator=(const char *chs)
{
    return setLatin1(chs);
}

DeprecatedString &DeprecatedString::operator=(DeprecatedChar qc)
{
    return *this = DeprecatedString(qc);
}

DeprecatedString &DeprecatedString::operator=(char ch)
{
    return *this = DeprecatedString(DeprecatedChar(ch));
}

DeprecatedChar DeprecatedString::at(unsigned i) const
{
    DeprecatedStringData *thisData = *dataHandle;
    
    if (i >= thisData->_length)
        return 0;
        
    if (thisData->_isAsciiValid) {
        return thisData->_ascii[i];
    }
    
    ASSERT(thisData->_isUnicodeValid);
    return thisData->_unicode[i];
}

int DeprecatedString::compare(const DeprecatedString& s) const
{
    if (dataHandle[0]->_isAsciiValid && s.dataHandle[0]->_isAsciiValid)
        return strcmp(ascii(), s.ascii());
    return ucstrcmp(*this,s);
}

int DeprecatedString::compare(const char *chs) const
{
    if (!chs)
        return isEmpty() ? 0 : 1;
    DeprecatedStringData *d = dataHandle[0];
    if (d->_isAsciiValid)
        return strcmp(ascii(), chs);
    const DeprecatedChar *s = unicode();
    unsigned len = d->_length;
    for (unsigned i = 0; i != len; ++i) {
        char c2 = chs[i];
        if (!c2)
            return 1;
        DeprecatedChar c1 = s[i];
        if (c1.unicode() < c2)
            return -1;
        if (c1.unicode() > c2)
            return 1;
    }
    return chs[len] ? -1 : 0;
}

bool DeprecatedString::startsWith( const DeprecatedString& s ) const
{
    if (dataHandle[0]->_isAsciiValid){
        const char *asc = ascii();
        
        for ( int i =0; i < (int) s.dataHandle[0]->_length; i++ ) {
            if ( i >= (int) dataHandle[0]->_length || asc[i] != s[i] )
                return false;
        }
    }
    else if (dataHandle[0]->_isUnicodeValid){
        const DeprecatedChar *uni = unicode();
        
        for ( int i =0; i < (int) s.dataHandle[0]->_length; i++ ) {
            if ( i >= (int) dataHandle[0]->_length || uni[i] != s[i] )
                return false;
        }
    }
    else
        FATAL("invalid character cache");
        
    return true;
}

bool DeprecatedString::startsWith(const char *prefix) const
{
    DeprecatedStringData *data = *dataHandle;

    unsigned prefixLength = strlen(prefix);
    if (data->_isAsciiValid) {
        return strncmp(prefix, data->_ascii, prefixLength) == 0;
    } else {
        ASSERT(data->_isUnicodeValid);
        if (prefixLength > data->_length) {
            return false;
        }
        const DeprecatedChar *uni = data->_unicode;        
        for (unsigned i = 0; i < prefixLength; ++i) {
            if (uni[i] != prefix[i]) {
                return false;
            }
        }
        return true;
    }
}

bool DeprecatedString::startsWith(const char *prefix, bool caseSensitive) const
{
    if (caseSensitive) {
        return startsWith(prefix);
    }

    DeprecatedStringData *data = *dataHandle;

    unsigned prefixLength = strlen(prefix);
    if (data->_isAsciiValid) {
        return strncasecmp(prefix, data->_ascii, prefixLength) == 0;
    } else {
        ASSERT(data->_isUnicodeValid);
        if (prefixLength > data->_length) {
            return false;
        }
        const DeprecatedChar *uni = data->_unicode;        
        for (unsigned i = 0; i < prefixLength; ++i) {
            if (!equalCaseInsensitive(uni[i], prefix[i])) {
                return false;
            }
        }
        return true;
    }
}

bool DeprecatedString::endsWith(const DeprecatedString& s) const
{
    const DeprecatedChar *uni = unicode();

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

bool DeprecatedString::isNull() const
{
    return dataHandle == shared_null_handle;
}

int DeprecatedString::find(DeprecatedChar qc, int index) const
{
    if (dataHandle[0]->_isAsciiValid) {
        if (!IS_ASCII_QCHAR(qc))
            return -1;
        return find(qc.unicode(), index);
    }
    return find(DeprecatedString(qc), index, true);
}

int DeprecatedString::find(char ch, int index) const
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
        return find(DeprecatedChar(ch), index, true);
    else
        FATAL("invalid character cache");

    return -1;
}

int DeprecatedString::find(const DeprecatedString &str, int index, bool caseSensitive) const
{
    // FIXME, use the first character algorithm
    /*
      We use some weird hashing for efficiency's sake.  Instead of
      comparing strings, we compare the sum of str with that of
      a part of this DeprecatedString.  Only if that matches, we call memcmp
      or ucstrnicmp.

      The hash value of a string is the sum of the cells of its
      QChars.
    */
    if ( index < 0 )
        index += dataHandle[0]->_length;
    int lstr = str.dataHandle[0]->_length;
    int lthis = dataHandle[0]->_length - index;
    if ( (unsigned)lthis > dataHandle[0]->_length )
        return -1;
    int delta = lthis - lstr;
    if ( delta < 0 )
        return -1;

    const DeprecatedChar *uthis = unicode() + index;
    const DeprecatedChar *ustr = str.unicode();
    unsigned hthis = 0;
    unsigned hstr = 0;
    int i;
    if ( caseSensitive ) {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[i].unicode();
            hstr += ustr[i].unicode();
        }
        i = 0;
        while ( true ) {
            if ( hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(DeprecatedChar)) == 0 )
                return index + i;
            if ( i == delta )
                return -1;
            hthis += uthis[i + lstr].unicode();
            hthis -= uthis[i].unicode();
            i++;
        }
    } else {
        for ( i = 0; i < lstr; i++ ) {
            hthis += toASCIILower(uthis[i].unicode());
            hstr += toASCIILower(ustr[i].unicode());
        }
        i = 0;
        while ( true ) {
            if ( hthis == hstr && equalCaseInsensitive(uthis + i, ustr, lstr) )
                return index + i;
            if ( i == delta )
                return -1;
            hthis += toASCIILower(uthis[i + lstr].unicode());
            hthis -= toASCIILower(uthis[i].unicode());
            i++;
        }
    }
}

// This function should be as fast as possible, every little bit helps.
// Our usage patterns are typically small strings.  In time trials
// this simplistic algorithm is much faster than Boyer-Moore or hash
// based algorithms.
int DeprecatedString::find(const char *chs, int index, bool caseSensitive) const
{
    if (!chs || index < 0)
        return -1;

    DeprecatedStringData *data = *dataHandle;

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
            unsigned char lc = toASCIILower(*chs);
            do {
                if (toASCIILower(*++ptr) == lc && equalCaseInsensitive(ptr + 1, chsPlusOne, chsLengthMinusOne)) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        }
    } else {
        ASSERT(data->_isUnicodeValid);

        const DeprecatedChar *ptr = data->_unicode + index - 1;
        if (caseSensitive) {
            DeprecatedChar c = *chs;
            do {
                if (*++ptr == c && equal(ptr + 1, chsPlusOne, chsLengthMinusOne)) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        } else {
            unsigned char lc = toASCIILower(*chs);
            do {
                if (toASCIILower((++ptr)->unicode()) == lc && equalCaseInsensitive(ptr + 1, chsPlusOne, chsLengthMinusOne)) {
                    return data->_length - chsLength - n + 1;
                }
            } while (--n);
        }
    }

    return -1;
}

int DeprecatedString::find(const RegularExpression &qre, int index) const
{
    if ( index < 0 )
        index += dataHandle[0]->_length;
    return qre.match( *this, index );
}

int DeprecatedString::findRev(char ch, int index) const
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
        return findRev(DeprecatedString(DeprecatedChar(ch)), index);
    else
        FATAL("invalid character cache");

    return -1;
}

int DeprecatedString::findRev(const char *chs, int index) const
{
    return findRev(DeprecatedString(chs), index);
}

int DeprecatedString::findRev( const DeprecatedString& str, int index, bool cs ) const
{
    // FIXME, use the first character algorithm
    /*
      See DeprecatedString::find() for explanations.
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

    const DeprecatedChar *uthis = unicode();
    const DeprecatedChar *ustr = str.unicode();
    unsigned hthis = 0;
    unsigned hstr = 0;
    int i;
    if ( cs ) {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[index + i].unicode();
            hstr += ustr[i].unicode();
        }
        i = index;
        while ( true ) {
            if ( hthis == hstr && memcmp(uthis + i, ustr, lstr * sizeof(DeprecatedChar)) == 0 )
                return i;
            if ( i == 0 )
                return -1;
            i--;
            hthis -= uthis[i + lstr].unicode();
            hthis += uthis[i].unicode();
        }
    } else {
        for ( i = 0; i < lstr; i++ ) {
            hthis += uthis[index + i].lower().unicode();
            hstr += ustr[i].lower().unicode();
        }
        i = index;
        while ( true ) {
            if ( hthis == hstr && equalCaseInsensitive(uthis + i, ustr, lstr) )
                return i;
            if ( i == 0 )
                return -1;
            i--;
            hthis -= uthis[i + lstr].lower().unicode();
            hthis += uthis[i].lower().unicode();
        }
    }

    // Should never get here.
    return -1;
}


int DeprecatedString::contains(DeprecatedChar c, bool cs) const
{
    int count = 0;
    
    DeprecatedStringData *data = *dataHandle;
    
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
            unsigned char lc = toASCIILower(ac);
            while (n--) {
                count += toASCIILower(*cPtr++) == lc;
            }
        }
    } else {
        ASSERT(data->_isUnicodeValid);
        const DeprecatedChar *uc = data->_unicode;
        int n = data->_length;
        if (cs) {                                       // case sensitive
            while ( n-- )
                count += *uc++ == c;
        } else {                                        // case insensitive
            ::UChar lc = toASCIILower(c.unicode());
            while (n--) {
                count += toASCIILower(uc->unicode()) == lc;
                uc++;
            }
        }
    } 

    return count;
}

int DeprecatedString::contains(char ch) const
{
    return contains(DeprecatedChar(ch), true);
}

int DeprecatedString::contains(const char *str, bool caseSensitive) const
{
    if (!str)
        return 0;

    int len = strlen(str);
    char c = *str;

    DeprecatedStringData *data = *dataHandle;
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
            char lc = toASCIILower(c);
            do {
                count += toASCIILower(*p) == lc && equalCaseInsensitive(p + 1, str + 1, len - 1);
                p++;
            } while (--n);
        }
    } else {
        ASSERT(data->_isUnicodeValid);
        const DeprecatedChar *p = data->_unicode;
        if (caseSensitive) {
            do {
                count += *p == c && equal(p + 1, str + 1, len - 1);
                p++;
            } while (--n);
        } else {
            unsigned char lc = toASCIILower(c);
            do {
                count += toASCIILower(p->unicode()) == lc && equalCaseInsensitive(p + 1, str + 1, len - 1);
                p++;
            } while (--n);
        }
    }

    return count;
}

int DeprecatedString::contains(const DeprecatedString &str, bool caseSensitive) const
{
    if (str.isEmpty())
        return 0;

    const DeprecatedChar *strP = str.unicode();
    int len = str.dataHandle[0]->_length;
    DeprecatedChar c = *strP;

    const DeprecatedChar *p = unicode();
    int n = dataHandle[0]->_length;

    n -= len - 1;
    if (n <= 0)
        return 0;

    int count = 0;

    if (caseSensitive) {
        int byteCount = len * sizeof(DeprecatedChar);
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

bool DeprecatedString::isAllASCII() const
{
    DeprecatedStringData *data = *dataHandle;

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
        const DeprecatedChar *p = data->_unicode;
        while (n--) {
            if ((*p++).unicode() > 0x7F) {
                return false;
            }
        }
    }

    return true;
}

bool DeprecatedString::isAllLatin1() const
{
    DeprecatedStringData *data = *dataHandle;

    if (data->_isAsciiValid) {
        return true;
    }

    ASSERT(data->_isUnicodeValid);
    int n = data->_length;
    const DeprecatedChar *p = data->_unicode;
    while (n--) {
        if ((*p++).unicode() > 0xFF) {
            return false;
        }
    }

    return true;
}

bool DeprecatedString::hasFastLatin1() const
{
    DeprecatedStringData *data = *dataHandle;
    return data->_isAsciiValid;
}

void DeprecatedString::copyLatin1(char *buffer, unsigned position, unsigned maxLength) const
{
    DeprecatedStringData *data = *dataHandle;

    int length = data->_length;
    if (position > static_cast<unsigned>(length))
        length = 0;
    else
        length -= position;
    if (static_cast<unsigned>(length) > maxLength)
        length = static_cast<int>(maxLength);

    buffer[length] = 0;

    if (data->_isAsciiValid) {
        memcpy(buffer, data->_ascii + position, length);
        return;
    }

    ASSERT(data->_isUnicodeValid);
    const DeprecatedChar* uc = data->_unicode + position;
    while (length--)
        *buffer++ = (*uc++).latin1();
}

short DeprecatedString::toShort(bool *ok, int base) const
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

unsigned short DeprecatedString::toUShort(bool *ok, int base) const
{
    unsigned v = toUInt(ok, base);
    unsigned short sv = v;
    if (sv != v) {
        if (ok)
            *ok = false;
        return 0;
    }
    return sv;
}

template <typename IntegralType> static inline
IntegralType toIntegralType(const DeprecatedString& string, bool *ok, int base)
{
    static const IntegralType integralMax = std::numeric_limits<IntegralType>::max();
    static const bool isSigned = std::numeric_limits<IntegralType>::is_signed;
    const DeprecatedChar* p = string.unicode();
    const IntegralType maxMultiplier = integralMax / base;

    int length = string.length();
    IntegralType value = 0;
    bool isOk = false;
    bool isNegative = false;

    if (!p)
        goto bye;

    // skip leading whitespace
    while (length && p->isSpace()) {
        length--;
        p++;
    }

    if (isSigned && length && *p == '-') {
        length--;
        p++;
        isNegative = true;
    } else if (length && *p == '+') {
        length--;
        p++;
    }

    if (!length || !isCharacterAllowedInBase(*p, base))
        goto bye;

    while (length && isCharacterAllowedInBase(*p, base)) {
        length--;
        IntegralType digitValue;
        ::UChar c = p->unicode();
        if (isASCIIDigit(c))
            digitValue = c - '0';
        else if (c >= 'a')
            digitValue = c - 'a' + 10;
        else
            digitValue = c - 'A' + 10;

        if (value > maxMultiplier || (value == maxMultiplier && digitValue > (integralMax % base) + isNegative))
            goto bye;

        value = base * value + digitValue;
        p++;
    }

    if (isNegative)
        value = -value;

    // skip trailing space
    while (length && p->isSpace()) {
        length--;
        p++;
    }

    if (!length)
        isOk = true;
bye:
    if (ok)
        *ok = isOk;
    return isOk ? value : 0;
}

int DeprecatedString::toInt(bool *ok, int base) const
{
    return toIntegralType<int>(*this, ok, base);
}

int64_t DeprecatedString::toInt64(bool *ok, int base) const
{
    return toIntegralType<int64_t>(*this, ok, base);
}

unsigned DeprecatedString::toUInt(bool *ok, int base) const
{
    return toIntegralType<unsigned>(*this, ok, base);
}

uint64_t DeprecatedString::toUInt64(bool *ok, int base) const
{
    return toIntegralType<uint64_t>(*this, ok, base);
}

double DeprecatedString::toDouble(bool *ok) const
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

float DeprecatedString::toFloat(bool* ok) const
{
    // FIXME: this will return ok even when the string does not fit into a float
    return narrowPrecisionToFloat(toDouble(ok));
}

DeprecatedString DeprecatedString::left(unsigned len) const
{
    return mid(0, len);
}

DeprecatedString DeprecatedString::right(unsigned len) const
{
    return mid(length() - len, len);
}

DeprecatedString DeprecatedString::mid(unsigned start, unsigned len) const
{
    if (dataHandle && *dataHandle) {
        DeprecatedStringData &data = **dataHandle;
        
        // clip length
        if (start >= data._length)
            return DeprecatedString();
        
        if (len > data._length - start)
            len = data._length - start;

        if (len == 0)
            return DeprecatedString();
        
        if (start == 0 && len == data._length)
            return *this;

        ASSERT(start + len >= start &&       // unsigned overflow
               start + len <= data._length); // past the end
        
        // ascii case
        if (data._isAsciiValid && data._ascii)
            return DeprecatedString(&data._ascii[start] , len);
        
        // unicode case
        if (data._isUnicodeValid && data._unicode)
            return DeprecatedString(&data._unicode[start], len);
    }
    
    // degenerate case
    return DeprecatedString();
}

DeprecatedString DeprecatedString::copy() const
{
    // does not need to be a deep copy
    return DeprecatedString(*this);
}

DeprecatedString DeprecatedString::lower() const
{
    DeprecatedString s(*this);
    DeprecatedStringData *d = *s.dataHandle;
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
            DeprecatedChar *p = d->_unicode;
            while (l--) {
                DeprecatedChar c = *p;
                // FIXME: Doesn't work for 0x80-0xFF.
                if (IS_ASCII_QCHAR(c)) {
                    if (c.unicode() >= 'A' && c.unicode() <= 'Z') {
                        if (!detached) {
                            s.detach();
                            d = *s.dataHandle;
                            p = d->_unicode + d->_length - l - 1;
                            detached = true;
                        }
                        *p = c.unicode() + ('a' - 'A');
                    }
                } else {
                    DeprecatedChar clower = c.lower();
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

DeprecatedString DeprecatedString::stripWhiteSpace() const
{
    if ( isEmpty() )                            // nothing to do
        return *this;
    if ( !at(0).isSpace() && !at(dataHandle[0]->_length-1).isSpace() )
        return *this;

    int start = 0;
    int end = dataHandle[0]->_length - 1;

    DeprecatedString result = fromLatin1("");
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
            memcpy(result.forceUnicode(), &unicode()[start], sizeof(DeprecatedChar)*l );
    }
    else
        FATAL("invalid character cache");
    return result;
}

DeprecatedString DeprecatedString::simplifyWhiteSpace() const
{
    if ( isEmpty() )                            // nothing to do
        return *this;
    
    DeprecatedString result;

    if (dataHandle[0]->_isAsciiValid){
        result.setLength( dataHandle[0]->_length );
        const char *from = ascii();
        const char *fromend = from + dataHandle[0]->_length;
        int outc=0;
        
        char *to = const_cast<char*>(result.ascii());
        while ( true ) {
            while ( from!=fromend && DeprecatedChar(*from).isSpace() )
                from++;
            while ( from!=fromend && !DeprecatedChar(*from).isSpace() )
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
        const DeprecatedChar *from = unicode();
        const DeprecatedChar *fromend = from + dataHandle[0]->_length;
        int outc=0;
        
        DeprecatedChar *to = result.forceUnicode();
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

void DeprecatedString::deref()
{
    dataHandle[0]->deref();
}


DeprecatedString &DeprecatedString::setUnicode(const DeprecatedChar *uni, unsigned len)
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
        *dataHandle = new DeprecatedStringData(uni, len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        if ( uni )
            memcpy( (void *)unicode(), uni, sizeof(DeprecatedChar)*len );
        dataHandle[0]->_length = len;
        dataHandle[0]->_isAsciiValid = 0;
    }
    
    return *this;
}


DeprecatedString &DeprecatedString::setLatin1(const char *str, int len)
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
        *dataHandle = new DeprecatedStringData(str,len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        strcpy(const_cast<char*>(ascii()), str );
        dataHandle[0]->_length = len;
        dataHandle[0]->_isUnicodeValid = 0;
    }
    return *this;
}

DeprecatedString &DeprecatedString::setNum(short n)
{
    return format("%d", n);
}

DeprecatedString &DeprecatedString::setNum(unsigned short n)
{
    return format("%u", n);
}

DeprecatedString &DeprecatedString::setNum(int n)
{
    return format("%d", n);
}

DeprecatedString &DeprecatedString::setNum(unsigned n)
{
    return format("%u", n);
}

DeprecatedString &DeprecatedString::setNum(long n)
{
    return format("%ld", n);
}

DeprecatedString &DeprecatedString::setNum(unsigned long n)
{
    return format("%lu", n);
}

DeprecatedString &DeprecatedString::setNum(double n)
{
    return format("%.6lg", n);
}

DeprecatedString &DeprecatedString::format(const char *format, ...)
{
    // FIXME: this needs the same windows compat fixes as String::format

    va_list args;
    va_start(args, format);
    
    // Do the format once to get the length.
#if COMPILER(MSVC) 
    int result = _vscprintf(format, args);
#else
    char ch;
    int result = vsnprintf(&ch, 1, format, args);
#endif
    
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
        *dataHandle = new DeprecatedStringData((char *)0, len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        dataHandle[0]->_length = len;
        dataHandle[0]->_isUnicodeValid = 0;
    }

    // Now do the formatting again, guaranteed to fit.
    vsprintf(const_cast<char*>(ascii()), format, args);

    va_end(args);
    return *this;
}

DeprecatedString &DeprecatedString::prepend(const DeprecatedString &qs)
{
    return insert(0, qs);
}

DeprecatedString &DeprecatedString::prepend(const DeprecatedChar *characters, unsigned length)
{
    return insert(0, characters, length);
}

DeprecatedString &DeprecatedString::append(const DeprecatedString &qs)
{
    return insert(dataHandle[0]->_length, qs);
}

DeprecatedString &DeprecatedString::append(const char *characters, unsigned length)
{
    return insert(dataHandle[0]->_length, characters, length);
}

DeprecatedString &DeprecatedString::append(const DeprecatedChar *characters, unsigned length)
{
    return insert(dataHandle[0]->_length, characters, length);
}

DeprecatedString &DeprecatedString::insert(unsigned index, const char *insertChars, unsigned insertLength)
{
    if (insertLength == 0)
        return *this;
        
    detach();
    
    if (dataHandle[0]->_isAsciiValid){
        unsigned originalLength = dataHandle[0]->_length;
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
        unsigned originalLength = dataHandle[0]->_length;
        DeprecatedChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        targetChars = (DeprecatedChar *)unicode();
        
        // Move tail to make space for inserted characters.
        memmove (targetChars+(index+insertLength), targetChars+index, (originalLength-index)*sizeof(DeprecatedChar));

        // Insert characters.
        unsigned i = insertLength;
        DeprecatedChar *target = targetChars+index;
        
        while (i--)
            *target++ = *insertChars++;        
    }
    else
        FATAL("invalid character cache");
    
    return *this;
}


DeprecatedString &DeprecatedString::insert(unsigned index, const DeprecatedString &qs)
{
    if (qs.dataHandle[0]->_length == 0)
        return *this;
        
    if (dataHandle[0]->_isAsciiValid && qs.isAllLatin1()) {
        insert(index, qs.latin1(), qs.length());
    }
    else {
        unsigned insertLength = qs.dataHandle[0]->_length;
        unsigned originalLength = dataHandle[0]->_length;

        forceUnicode();
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        DeprecatedChar *targetChars = const_cast<DeprecatedChar *>(unicode());
        
        // Move tail to make space for inserted characters.
        memmove (targetChars+(index+insertLength), targetChars+index, (originalLength-index)*sizeof(DeprecatedChar));

        // Insert characters.
        if (qs.dataHandle[0]->_isAsciiValid){
            unsigned i = insertLength;
            DeprecatedChar *target = targetChars+index;
            char *a = const_cast<char*>(qs.ascii());
            
            while (i--)
                *target++ = *a++;
        }
        else {
            DeprecatedChar *insertChars = (DeprecatedChar *)qs.unicode();
            memcpy (targetChars+index, insertChars, insertLength*sizeof(DeprecatedChar));
        }
        
        dataHandle[0]->_isAsciiValid = 0;
    }
    
    return *this;
}


DeprecatedString &DeprecatedString::insert(unsigned index, const DeprecatedChar *insertChars, unsigned insertLength)
{
    if (insertLength == 0)
        return *this;
        
    forceUnicode();
    
    unsigned originalLength = dataHandle[0]->_length;
    setLength(originalLength + insertLength);

    DeprecatedChar *targetChars = const_cast<DeprecatedChar *>(unicode());
    if (originalLength > index) {
        memmove(targetChars + index + insertLength, targetChars + index, (originalLength - index) * sizeof(DeprecatedChar));
    }
    memcpy(targetChars + index, insertChars, insertLength * sizeof(DeprecatedChar));
    
    return *this;
}


DeprecatedString &DeprecatedString::insert(unsigned index, DeprecatedChar qc)
{
    detach();
    
    if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(qc)){
        unsigned originalLength = dataHandle[0]->_length;
        char insertChar = qc.unicode();
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
        unsigned originalLength = dataHandle[0]->_length;
        
        forceUnicode();

        // Ensure that we have enough space.
        setLength (originalLength + 1);
        DeprecatedChar *targetChars = const_cast<DeprecatedChar *>(unicode());
        
        // Move tail to make space for inserted character.
        memmove (targetChars+(index+1), targetChars+index, (originalLength-index)*sizeof(DeprecatedChar));

        targetChars[index] = qc;
    }
    
    return *this;
}


DeprecatedString &DeprecatedString::insert(unsigned index, char ch)
{
    detach();
    
    if (dataHandle[0]->_isAsciiValid) {
        unsigned originalLength = dataHandle[0]->_length;
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
        unsigned originalLength = dataHandle[0]->_length;
        DeprecatedChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = (DeprecatedChar *)unicode();
        
        // Move tail to make space for inserted character.
        memmove (targetChars+(index+1), targetChars+index, (originalLength-index)*sizeof(DeprecatedChar));

        targetChars[index] = (DeprecatedChar)ch;
    }
    else
        FATAL("invalid character cache");
    
    return *this;
}

// Copy DeprecatedStringData if necessary. Must be called before the string data is mutated.
void DeprecatedString::detach()
{
    DeprecatedStringData *oldData = *dataHandle;

    if (oldData->refCount == 1 && oldData != shared_null)
        return;

    // Copy data for this string so we can safely mutate it.
    DeprecatedStringData *newData;
    if (oldData->_isAsciiValid)
        newData = new DeprecatedStringData(oldData->ascii(), oldData->_length);
    else
        newData = new DeprecatedStringData(oldData->unicode(), oldData->_length);
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

void DeprecatedString::detachAndDiscardCharacters()
{
    // Missing optimization: Don't bother copying the old data if we detach.
    detach();
}

DeprecatedString &DeprecatedString::remove(unsigned index, unsigned len)
{
    unsigned olen = dataHandle[0]->_length;
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
                    sizeof(DeprecatedChar)*(olen-index-len) );
            setLength( olen-len );
        }
        else
            FATAL("invalid character cache");
    }
    return *this;
}

DeprecatedString &DeprecatedString::replace(unsigned index, unsigned len, const DeprecatedString& str)
{
    return remove(index, len).insert(index, str);
}

DeprecatedString &DeprecatedString::replace(char pattern, const DeprecatedString &str)
{
    int slen = str.dataHandle[0]->_length;
    int index = 0;
    while ((index = find(pattern, index)) >= 0) {
        replace(index, 1, str);
        index += slen;
    }
    return *this;
}

DeprecatedString &DeprecatedString::replace(DeprecatedChar pattern, const DeprecatedString &str)
{
    int slen = str.dataHandle[0]->_length;
    int index = 0;
    while ((index = find(pattern, index)) >= 0) {
        replace(index, 1, str);
        index += slen;
    }
    return *this;
}

DeprecatedString &DeprecatedString::replace(const DeprecatedString &pattern, const DeprecatedString &str)
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


DeprecatedString &DeprecatedString::replace(const RegularExpression &qre, const DeprecatedString &str)
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


DeprecatedString &DeprecatedString::replace(DeprecatedChar oldChar, DeprecatedChar newChar)
{
    if (oldChar != newChar && find(oldChar) != -1) {
        unsigned length = dataHandle[0]->_length;
        
        detach();
        if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(newChar)) {
            char *p = const_cast<char *>(ascii());
            dataHandle[0]->_isUnicodeValid = 0;
            char oldC = oldChar.unicode();
            char newC = newChar.unicode();
            for (unsigned i = 0; i != length; ++i) {
                if (p[i] == oldC) {
                    p[i] = newC;
                }
            }
        } else {
            DeprecatedChar *p = const_cast<DeprecatedChar *>(unicode());
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


DeprecatedChar *DeprecatedString::forceUnicode()
{
    detach();
    DeprecatedChar *result = const_cast<DeprecatedChar *>(unicode());
    dataHandle[0]->_isAsciiValid = 0;
    return result;
}


// Increase buffer size if necessary.  Newly allocated
// bytes will contain garbage.
void DeprecatedString::setLength(unsigned newLen)
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

    if (dataHandle[0]->_isUnicodeValid){
        if (newLen > dataHandle[0]->_maxUnicode) {
            dataHandle[0]->increaseUnicodeSize(newLen);
        }
    }

    dataHandle[0]->_length = newLen;
}


void DeprecatedString::truncate(unsigned newLen)
{
    if ( newLen < dataHandle[0]->_length )
        setLength( newLen );
}

void DeprecatedString::fill(DeprecatedChar qc, int len)
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
                *nd++ = qc.unicode();
            dataHandle[0]->_isUnicodeValid = 0;
        } else {
            setLength(len);
            DeprecatedChar *nd = forceUnicode();
            while (len--) 
                *nd++ = qc;
        }
    }
}

DeprecatedString &DeprecatedString::append(DeprecatedChar qc)
{
    detach();
    
    DeprecatedStringData *thisData = *dataHandle;
    if (thisData->_isUnicodeValid && thisData->_length + 1 < thisData->_maxUnicode){
        thisData->_unicode[thisData->_length] = qc;
        thisData->_length++;
        thisData->_isAsciiValid = 0;
        return *this;
    }
    else if (thisData->_isAsciiValid && IS_ASCII_QCHAR(qc) && thisData->_length + 2 < thisData->_maxAscii){
        thisData->_ascii[thisData->_length] = qc.unicode();
        thisData->_length++;
        thisData->_ascii[thisData->_length] = 0;
        thisData->_isUnicodeValid = 0;
        return *this;
    }
    return insert(thisData->_length, qc);
}

DeprecatedString &DeprecatedString::append(char ch)
{
    detach();
    
    DeprecatedStringData *thisData = *dataHandle;
    if (thisData->_isUnicodeValid && thisData->_length + 1 < thisData->_maxUnicode){
        thisData->_unicode[thisData->_length] = (DeprecatedChar)ch;
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

void DeprecatedString::reserve(unsigned length)
{
    if (length > dataHandle[0]->_maxUnicode) {
        detach();
        dataHandle[0]->increaseUnicodeSize(length);
    }
}

bool operator==(const DeprecatedString &s1, const DeprecatedString &s2)
{
    if (s1.dataHandle[0]->_isAsciiValid && s2.dataHandle[0]->_isAsciiValid) {
        return strcmp(s1.ascii(), s2.ascii()) == 0;
    }
    return s1.dataHandle[0]->_length == s2.dataHandle[0]->_length
        && memcmp(s1.unicode(), s2.unicode(), s1.dataHandle[0]->_length * sizeof(DeprecatedChar)) == 0;
}

bool operator==(const DeprecatedString &s1, const char *chs)
{
    if (!chs)
        return s1.isNull();
    DeprecatedStringData *d = s1.dataHandle[0];
    unsigned len = d->_length;
    if (d->_isAsciiValid) {
        const char *s = s1.ascii();
        for (unsigned i = 0; i != len; ++i) {
            char c = chs[i];
            if (!c || s[i] != c)
                return false;
        }
    } else {
        const DeprecatedChar *s = s1.unicode();
        for (unsigned i = 0; i != len; ++i) {
            char c = chs[i];
            if (!c || s[i] != c)
                return false;
        }
    }
    return chs[len] == '\0';
}

DeprecatedString operator+(const DeprecatedString &qs1, const DeprecatedString &qs2)
{
    return DeprecatedString(qs1) += qs2;
}

DeprecatedString operator+(const DeprecatedString &qs, const char *chs)
{
    return DeprecatedString(qs) += chs;
}

DeprecatedString operator+(const DeprecatedString &qs, DeprecatedChar qc)
{
    return DeprecatedString(qs) += qc;
}

DeprecatedString operator+(const DeprecatedString &qs, char ch)
{
    return DeprecatedString(qs) += ch;
}

DeprecatedString operator+(const char *chs, const DeprecatedString &qs)
{
    return DeprecatedString(chs) += qs;
}

DeprecatedString operator+(DeprecatedChar qc, const DeprecatedString &qs)
{
    return DeprecatedString(qc) += qs;
}

DeprecatedString operator+(char ch, const DeprecatedString &qs)
{
    return DeprecatedString(DeprecatedChar(ch)) += qs;
}

DeprecatedConstString::DeprecatedConstString(const DeprecatedChar* unicode, unsigned length) :
    DeprecatedString(new DeprecatedStringData((DeprecatedChar *)unicode, length, length), true)
{
}

DeprecatedConstString::~DeprecatedConstString()
{
    DeprecatedStringData *data = *dataHandle;
    if (data->refCount > 1) {
        DeprecatedChar *tp;
        if (data->_length <= WEBCORE_DS_INTERNAL_BUFFER_UCHARS) {
            data->_maxUnicode = WEBCORE_DS_INTERNAL_BUFFER_UCHARS;
            tp = (DeprecatedChar *)&data->_internalBuffer[0];
        } else {
            data->_maxUnicode = ALLOC_QCHAR_GOOD_SIZE(data->_length);
            tp = WEBCORE_ALLOCATE_CHARACTERS(data->_maxUnicode);
        }
        memcpy(tp, data->_unicode, data->_length * sizeof(DeprecatedChar));
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

#ifndef CHECK_FOR_HANDLE_LEAKS

static const size_t pageSize = 4096;
static const uintptr_t pageMask = ~(pageSize - 1);
static const size_t nodeBlockSize = pageSize / sizeof(HandleNode);

static HandleNode *initializeHandleNodeBlock(HandlePageNode *pageNode)
{
    unsigned i;
    HandleNode* block;
    HandleNode* aNode;

#if PLATFORM(WIN_OS)
    block = (HandleNode*)VirtualAlloc(0, pageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
#elif PLATFORM(SYMBIAN)
    // symbian::fixme needs to do page aligned allocation as valloc is not supported.
    block = NULL;
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
        block[0].type.freeNodes = block + allocated->type.internalNode.previous;
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

#endif

void freeHandle(DeprecatedStringData **_free)
{
#ifdef CHECK_FOR_HANDLE_LEAKS
    fastFree(_free);
    return;
#else

    HandleNode *free = (HandleNode *)_free;
    HandleNode *base = (HandleNode *)((uintptr_t)free & pageMask);
    HandleNode *freeNodes = base[0].type.freeNodes;
    HandlePageNode *pageNode = base[1].type.pageNode;
    
    if (freeNodes == 0){
        free->type.internalNode.previous = 0;
    }
    else {
        // Insert at head of free list.
        free->type.internalNode.previous = freeNodes - base;
        freeNodes->type.internalNode.next = free - base;
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
#endif
}

DeprecatedString DeprecatedString::fromUtf8(const char *chs)
{
    return UTF8Encoding().decode(chs, strlen(chs)).deprecatedString();
}

DeprecatedString DeprecatedString::fromUtf8(const char *chs, int len)
{
    return UTF8Encoding().decode(chs, len).deprecatedString();
}

DeprecatedCString DeprecatedString::utf8(int& length) const
{
    DeprecatedCString result = UTF8Encoding().encode((::UChar*)unicode(), this->length()).deprecatedCString();
    length = result.length();
    return result;
}

DeprecatedString::DeprecatedString(const Identifier& str)
{
    if (str.isNull()) {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else {
        dataHandle = allocateHandle();
        *dataHandle = &internalData;
        internalData.initialize(reinterpret_cast<const DeprecatedChar*>(str.data()), str.size());
    }
}

DeprecatedString::DeprecatedString(const UString& str)
{
    if (str.isNull()) {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else {
        dataHandle = allocateHandle();
        *dataHandle = &internalData;
        internalData.initialize(reinterpret_cast<const DeprecatedChar*>(str.data()), str.size());
    }
}

#if PLATFORM(QT)
DeprecatedString::DeprecatedString(const QString& str)
{
    if (str.isNull()) {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else {
        dataHandle = allocateHandle();
        *dataHandle = &internalData;
        internalData.initialize(reinterpret_cast<const DeprecatedChar*>(str.data()), str.length());
    }
}
#endif

DeprecatedString::operator Identifier() const
{
    if (isNull())
        return Identifier();
    return Identifier(reinterpret_cast<const KJS::UChar*>(unicode()), length());
}

DeprecatedString::operator UString() const
{
    if (isNull())
        return UString();
    return UString(reinterpret_cast<const KJS::UChar*>(unicode()), length());
}

bool equalIgnoringCase(const DeprecatedString& a, const DeprecatedString& b)
{
    unsigned len = a.length();
    if (len != b.length())
        return false;

    DeprecatedStringData* dataA = a.dataHandle[0];
    DeprecatedStringData* dataB = b.dataHandle[0];

    if (dataA->_isAsciiValid != dataB->_isAsciiValid)
        return false;

    if (dataA->_isAsciiValid && dataB->_isAsciiValid)
        return strncasecmp(dataA->_ascii, dataB->_ascii, len) == 0;

    ASSERT(dataA->_isUnicodeValid);
    ASSERT(dataB->_isUnicodeValid);
    return equalCaseInsensitive(dataA->_unicode, dataB->_unicode, len);
}

} // namespace WebCore
