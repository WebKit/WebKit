/*
 * Copyright (C) 2001, 2002 Apple Computer, Inc.  All rights reserved.
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

#import <Foundation/Foundation.h>
#import <kwqdebug.h>
#import <qstring.h>
#import <qregexp.h>
#import <stdio.h>

// Why can't I find this in a header anywhere?  It's too bad we have
// to wire knowledge of allocation sizes, but it makes a huge diffence.
extern "C" {
int malloc_good_size(int size);
}

#define ALLOC_QCHAR_GOOD_SIZE(X) (malloc_good_size(X*sizeof(QChar))/sizeof(QChar))
#define ALLOC_CHAR_GOOD_SIZE(X) (malloc_good_size(X))

#ifdef QSTRING_DEBUG_ALLOCATIONS
#import <pthread.h>
#import <mach/mach_types.h>

static CFMutableDictionaryRef _allocatedBuffers = 0;
#define ALLOCATION_HISTOGRAM_SIZE 128
static uint _allocationHistogram[ALLOCATION_HISTOGRAM_SIZE];

static uint stackInstances = 0;
static uint heapInstances = 0;
static uint stringDataInstances = 0;
static uint stringDataHeapInstances = 0;
static uint stringDataDetachments = 0;
static uint handleInstances = 0;

static bool _isOnStack(void *ptr){
    void *address;
    size_t size;
    pthread_t thisThread = pthread_self();
    
    size = pthread_get_stacksize_np(thisThread);
    address = pthread_get_stackaddr_np(thisThread);
    if (ptr >= (void *)(((char *)address) - size) &&
        ptr <= address)
        return true;
    return false;
}


static void countInstance(void *ptr)
{
    if (_isOnStack(ptr))
        stackInstances++;
    else
        heapInstances++;
}

static CFMutableDictionaryRef allocatedBuffers()
{
    if (_allocatedBuffers == 0){
        for (int i = 0; i < ALLOCATION_HISTOGRAM_SIZE; i++)
            _allocationHistogram[i] = 0;
        _allocatedBuffers = CFDictionaryCreateMutable (kCFAllocatorDefault, 1024*8, NULL, NULL);
    }
    return _allocatedBuffers;
}

static char *ALLOC_CHAR(int n){
    char *ptr = (char *)malloc(n);

    CFDictionarySetValue (allocatedBuffers(), ptr, (void *)n);
    
    if (n >= ALLOCATION_HISTOGRAM_SIZE)
        _allocationHistogram[ALLOCATION_HISTOGRAM_SIZE-1]++;
    else
        _allocationHistogram[n]++;
    return ptr;
}

static char *REALLOC_CHAR(void *p, int n){
    char *ptr = (char *)realloc(p, n);

    CFDictionaryRemoveValue (allocatedBuffers(), p);
    CFDictionarySetValue (allocatedBuffers(), ptr, (const void *)(n));
    if (n >= ALLOCATION_HISTOGRAM_SIZE)
        _allocationHistogram[ALLOCATION_HISTOGRAM_SIZE-1]++;
    else
        _allocationHistogram[n]++;
    return ptr;
}

static void DELETE_CHAR(void *p)
{
    CFDictionaryRemoveValue (allocatedBuffers(), p);
    free (p);
}

static QChar *ALLOC_QCHAR(int n){
    size_t size = (sizeof(QChar)*( n ));
    QChar *ptr = (QChar *)malloc(size);

    CFDictionarySetValue (allocatedBuffers(), ptr, (const void *)size);
    if (size >= ALLOCATION_HISTOGRAM_SIZE)
        _allocationHistogram[ALLOCATION_HISTOGRAM_SIZE-1]++;
    else
        _allocationHistogram[size]++;
    return ptr;
}

static QChar *REALLOC_QCHAR(void *p, int n){
    size_t size = (sizeof(QChar)*( n ));
    QChar *ptr = (QChar *)realloc(p, size);

    CFDictionaryRemoveValue (allocatedBuffers(), p);
    CFDictionarySetValue (allocatedBuffers(), ptr, (const void *)size);
    if (size >= ALLOCATION_HISTOGRAM_SIZE)
        _allocationHistogram[ALLOCATION_HISTOGRAM_SIZE-1]++;
    else
        _allocationHistogram[size]++;
        
    return ptr;
}

static void DELETE_QCHAR(void *p)
{
    CFDictionaryRemoveValue (allocatedBuffers(), p);
    free (p);
}

void _printQStringAllocationStatistics()
{
    const void **values;
    const void **keys;
    int j, i, count;
    int totalSize = 0;
    int totalAllocations = 0;
    
    count = (int)CFDictionaryGetCount (allocatedBuffers());
    values = (const void **)malloc (count*sizeof(void *));
    keys = (const void **)malloc (count*sizeof(void *));

    CFDictionaryGetKeysAndValues (allocatedBuffers(), keys, values);
    printf ("Leaked strings:\n");
    for (i = 0; i < count; i++){
        char *cp = (char *)keys[i];
        printf ("%04d:  0x%08x size %d \"", i, (unsigned int)keys[i], (unsigned int)values[i]);
        for (j = 0; j < MIN ((int)values[i], 64); j++){
            if (isprint(*cp))
                putchar (*cp);
            cp++;
        }
        printf ("\"\n");
        totalSize += (int)values[i];
    }
    printf ("Total leak %d\n", totalSize);
    
    printf ("\nString size histogram:\n");
    for (i = 0; i < ALLOCATION_HISTOGRAM_SIZE; i++){
        if (_allocationHistogram[i])
            printf ("[%d] = %d\n", i, _allocationHistogram[i]);
        totalAllocations += _allocationHistogram[i];
    }
    printf ("Total allocations %d\n", totalAllocations);
    
    printf ("\nQString instance counts:\n");
    printf ("QString stack allocated instances %d\n", stackInstances);
    printf ("QString heap allocated instances %d\n", heapInstances);
    printf ("QStringData instances %d\n", stringDataInstances);
    printf ("QStringData heap allocated instances %d\n", stringDataHeapInstances);
    printf ("QStringData detachments (copies) %d\n", stringDataDetachments);
    printf ("QStringData handles %d\n", handleInstances);
    
    free(keys);
    free(values);
}
#else

#define ALLOC_CHAR( N ) (char*) malloc(N)
#define REALLOC_CHAR( P, N ) realloc(P,N)
#define DELETE_CHAR( P ) free(P)

#define ALLOC_QCHAR( N ) (QChar*) malloc(sizeof(QChar)*( N ))
#define REALLOC_QCHAR( P, N ) realloc(P,sizeof(QChar)*( N ))
#define DELETE_QCHAR( P ) free( P )
#endif // QSTRING_DEBUG_ALLOCATIONS

#import <mach/vm_map.h>
#import <mach/mach_init.h>

static void *allocateHandle();
static void freeHandle(void *free);

#define IS_ASCII_QCHAR(c) ((c).unicode() > 0 && (c).unicode() <= 0xff)

static const int caseDelta = ('a' - 'A');
#define ASCII_TO_LOWER (p) ((p >= 'A' && p <= 'Z') ? (p + caseDelta) : p)

#ifdef NDEBUG
#define QSTRING_FAILURE(str_expr) ((void)0)
#else
#define QSTRING_FAILURE(str_expr) \
    do { \
        struct rlimit _rlimit = {RLIM_INFINITY, RLIM_INFINITY}; \
        setrlimit(RLIMIT_CORE, &_rlimit); \
        fprintf(stderr, "QSTRING FAILURE: (%s:%d %s) %s\n", __FILE__, __LINE__, __PRETTY_FUNCTION__, str_expr); \
        raise(SIGQUIT); \
    } while (0)
#endif //NDEBUG


// -------------------------------------------------------------------------
// Utility functions
// -------------------------------------------------------------------------

static int ucstrcmp( const QString &as, const QString &bs )
{
    const QChar *a = as.unicode();
    const QChar *b = bs.unicode();
    if ( a == b )
	return 0;
    if ( a == 0 )
	return 1;
    if ( b == 0 )
	return -1;
    int l = QMIN(as.length(), bs.length());
    while ( l-- && *a == *b )
	a++,b++;
    if ( l == -1 )
	return ( as.length() - bs.length() );
    return a->unicode() - b->unicode();
}


static int ucstrncmp( const QChar *a, const QChar *b, int l )
{
    while ( l-- && *a == *b )
	a++,b++;
    if ( l == -1 )
	return 0;
    return a->unicode() - b->unicode();
}


static int ucstrnicmp( const QChar *a, const QChar *b, int l )
{
    while ( l-- && a->lower() == b->lower() )
	a++,b++;
    if ( l == -1 )
	return 0;
    QChar al = a->lower();
    QChar bl = b->lower();
    return al.unicode() - bl.unicode();
}


static bool ok_in_base( QChar c, int base )
{
    if ( base <= 10 )
	return c.isDigit() && c.digitValue() < base;
    else
	return c.isDigit() || (c >= 'a' && c < char('a' + base - 10))
			   || (c >= 'A' && c < char('A' + base - 10));
}



// -------------------------------------------------------------------------
// QStringData
// -------------------------------------------------------------------------

// FIXME, make constructor explicity take a 'copy' flag.
// This can be used to hand off ownership of allocated data when detaching and
// deleting QStrings.

QStringData::QStringData() :
	refCount(1), _length(0), _unicode(0), _ascii(0), _maxUnicode(QS_INTERNAL_BUFFER_UCHARS), _isUnicodeInternal(0), _isUnicodeValid(0), _isHeapAllocated(0), _maxAscii(QS_INTERNAL_BUFFER_CHARS), _isAsciiInternal(1), _isAsciiValid(1) 
{ 
#ifdef QSTRING_DEBUG_ALLOCATIONS
    stringDataInstances++;
#endif
    _ascii = &_internalBuffer[0];
    _internalBuffer[0] = 0;
}

void QStringData::initialize()
{
    refCount = 1;
    _length = 0;
    _unicode = 0;
    _ascii = &_internalBuffer[0];
    _maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
    _isUnicodeInternal = 0;
    _isUnicodeValid = 0;
    _maxAscii = QS_INTERNAL_BUFFER_CHARS;
    _isAsciiInternal = 1;
    _isAsciiValid = 1;
    _internalBuffer[0] = 0;
    _isHeapAllocated = 0;
}

// Don't copy data.
QStringData::QStringData(QChar *u, uint l, uint m) :
	refCount(1), _length(l), _unicode(u), _ascii(0), _maxUnicode(m), _isUnicodeInternal(0), _isUnicodeValid(1), _isHeapAllocated(0), _maxAscii(QS_INTERNAL_BUFFER_CHARS), _isAsciiInternal(0), _isAsciiValid(0)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    stringDataInstances++;
#endif
}

// Don't copy data.
void QStringData::initialize(QChar *u, uint l, uint m)
{
    refCount = 1;
    _length = l;
    _unicode = u;
    _ascii = 0;
    _maxUnicode = m;
    _isUnicodeInternal = 0;
    _isUnicodeValid = 1;
    _maxAscii = 0;
    _isAsciiInternal = 0;
    _isAsciiValid = 0;
    _isHeapAllocated = 0;
}

// Copy data
QStringData::QStringData(const QChar *u, uint l)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    stringDataInstances++;
#endif
    initialize (u, l);
}

#ifdef QSTRING_DEBUG_ALLOCATIONS
void* QStringData::operator new(size_t s)
{
    stringDataHeapInstances++;
    return malloc(s);
}
void QStringData::operator delete(void*p)
{
    return free(p);
}
#endif

// Copy data
void QStringData::initialize(const QChar *u, uint l)
{
    refCount = 1;
    _length = l;
    _ascii = 0;
    _isUnicodeInternal = 0;
    _isUnicodeValid = 1;
    _maxAscii = 0;
    _isAsciiInternal = 0;
    _isAsciiValid = 0;
    _isHeapAllocated = 0;

    if (l > QS_INTERNAL_BUFFER_UCHARS){
        _maxUnicode = ALLOC_QCHAR_GOOD_SIZE(l);
        _unicode = ALLOC_QCHAR(_maxUnicode);
        if (u)
            memcpy(_unicode, u, l*sizeof(QChar));
    }
    else {
        _unicode = (QChar *)&_internalBuffer[0];
        _maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
        _isUnicodeInternal = 1;
        if (u)
            memcpy(_internalBuffer, u, l*sizeof(QChar));
    }
}


// Copy data
QStringData::QStringData(const char *a, uint l)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    stringDataInstances++;
#endif
    initialize(a, l);
}


// Copy data
void QStringData::initialize(const char *a, uint l)
{
    refCount = 1;
    _length = l;
    _unicode = 0;
    _isUnicodeInternal = 0;
    _isUnicodeValid = 0;
    _maxUnicode = 0;
    _isAsciiInternal = 0;
    _isAsciiValid = 1;
    _isHeapAllocated = 0;

    if (l > QS_INTERNAL_BUFFER_CHARS){
        _maxAscii = ALLOC_CHAR_GOOD_SIZE(l+1);
        _ascii = ALLOC_CHAR(_maxAscii);
        if (a)
            memcpy(_ascii, a, l*sizeof(char));
        _ascii[l] = 0;
    }
    else {
        _ascii = &_internalBuffer[0];
        _maxAscii = QS_INTERNAL_BUFFER_CHARS;
        _isAsciiInternal = 1;
        if (a)
            memcpy(_internalBuffer, a, l*sizeof(char));
        _internalBuffer[l] = 0;
    }
}


QStringData *QString::shared_null = 0;
QStringData **QString::shared_null_handle = 0;
QStringData **QString::makeSharedNullHandle()
{
    if (!shared_null_handle){
        shared_null_handle = (QStringData **)allocateHandle();
        *shared_null_handle = makeSharedNull();
    }
    return shared_null_handle;
}

QStringData* QString::makeSharedNull()
{
    if (!shared_null){
        shared_null = new QStringData();
        shared_null->ref();
        shared_null->_length = 0;
        shared_null->_maxAscii = 0;
        shared_null->_maxUnicode = 0;
        shared_null->_unicode = (QChar *)&shared_null->_internalBuffer[0]; 
        shared_null->_isUnicodeValid = 1;   
        shared_null->_isUnicodeInternal = 1;   
    }
    return shared_null;
}

QStringData::~QStringData()
{
    KWQ_ASSERT(refCount == 0);
        
    // Ack!  The destcructor will be called when the QString is deleted.
    if ( _unicode && !_isUnicodeInternal )
        DELETE_QCHAR (_unicode);
    if ( _ascii && !_isAsciiInternal )
        DELETE_CHAR (_ascii); 
}


inline char *QStringData::ascii()
{
    if (_isAsciiValid){
        if (_isAsciiInternal)
            return &_internalBuffer[0];
        else
            return _ascii;
    }
    else
        return makeAscii();
}


void QStringData::increaseAsciiSize(uint size)
{
    KWQ_ASSERT(this != QString::shared_null);
        
    uint newSize = (uint)ALLOC_CHAR_GOOD_SIZE(size);
    char *prev = 0;
    
    if (!_isAsciiValid && _isUnicodeValid)
        makeAscii();
        
    if (_ascii && !_isAsciiInternal)
        prev = _ascii;
        
    if (_isAsciiValid){
        if (_isAsciiInternal){
            if (_length){
                char *newAscii = ALLOC_CHAR(newSize);
                memcpy (newAscii, _ascii, _length);
                _ascii = newAscii;
            }
            else
                _ascii = ALLOC_CHAR(newSize);
        }
        else {
            _ascii = (char *)REALLOC_CHAR (_ascii, newSize);
            prev = 0;
        }
    }
    else
        QSTRING_FAILURE("invalid character cache");
    
    if (prev)
        DELETE_CHAR(prev);
        
    _maxAscii = newSize;
    _isAsciiValid = 1;
    _isAsciiInternal = 0;
    _isUnicodeValid = 0;
}


inline QChar *QStringData::unicode()
{
    if (_isUnicodeValid){
        if (_isUnicodeInternal)
            return (QChar *)_internalBuffer;
        else
            return _unicode;
    }
    else
        return makeUnicode();
}


void QStringData::increaseUnicodeSize(uint size)
{
    KWQ_ASSERT(this != QString::shared_null);
        
    uint newSize = (uint)ALLOC_QCHAR_GOOD_SIZE(size);
    QChar *prev = 0;
    
    if (!_isUnicodeValid && _isAsciiValid)
        makeUnicode();

    if (_unicode && !_isUnicodeInternal)
        prev = _unicode;
        
    if (_isUnicodeValid){
        if (_isUnicodeInternal){
            QChar *newUni = ALLOC_QCHAR(newSize);
            if (_length)
                memcpy (newUni, _unicode, _length*sizeof(QChar));
            _unicode = newUni;
        }
        else {
            _unicode = (QChar *)REALLOC_QCHAR (_unicode, newSize);
            prev = 0;
        }
    }
    else
        QSTRING_FAILURE("invalid character cache");
    
    if (prev)
        DELETE_QCHAR(prev);
        
    _maxUnicode = newSize;
    _isUnicodeValid = 1;
    _isUnicodeInternal = 0;
    _isAsciiValid = 0;
}


char *QStringData::makeAscii()
{
    KWQ_ASSERT(this != QString::shared_null);
        
    if (_isUnicodeValid){
        QChar copyBuf[QS_INTERNAL_BUFFER_CHARS];
        QChar *str;
        
        if (_ascii && !_isAsciiInternal)
            DELETE_QCHAR(_ascii);
            
        if (_length < QS_INTERNAL_BUFFER_CHARS){
            if (_isUnicodeInternal){
                uint i = _length;
                QChar *tp = &copyBuf[0], *fp = _unicode;
                while (i--)
                    *tp++ = *fp++;
                str = &copyBuf[0];
                _isUnicodeValid = 0;
            }
            else
                str = _unicode;
            _ascii = (char *)&_internalBuffer[0];
            _isAsciiInternal = 1;
        }
        else {
            uint newSize = ALLOC_CHAR_GOOD_SIZE(_length+1);
            _ascii = ALLOC_CHAR(newSize);
            _maxAscii = newSize;
            str = _unicode;
            _isAsciiInternal = 0;
        }

        uint i = _length;
        char *cp = _ascii;
        while ( i-- )
            *cp++ = *str++;
        *cp = 0;
        
        _isAsciiValid = 1;
    }
    else if (!_isAsciiValid)
        QSTRING_FAILURE("ASCII character cache not valid");
        
    return _ascii;
}


QChar *QStringData::makeUnicode()
{
    KWQ_ASSERT(this != QString::shared_null);
        
    if (_isAsciiValid){
        char copyBuf[QS_INTERNAL_BUFFER_CHARS];
        char *str;
        
        if (_unicode && !_isUnicodeInternal)
            DELETE_QCHAR(_unicode);
            
        if (_length <= QS_INTERNAL_BUFFER_UCHARS){
            if (_isAsciiInternal){
                uint i = _length;
                char *tp = &copyBuf[0], *fp = _ascii;
                while (i--)
                    *tp++ = *fp++;
                str = &copyBuf[0];
                _isAsciiValid = 0;
            }
            else
                str = _ascii;
            _unicode = (QChar *)&_internalBuffer[0];
            _isUnicodeInternal = 1;
        }
        else {
            uint newSize = ALLOC_QCHAR_GOOD_SIZE(_length);
            _unicode = ALLOC_QCHAR(newSize);
            _maxUnicode = newSize;
            str = _ascii;
            _isUnicodeInternal = 0;
        }
        uint i = _length;
        QChar *cp = _unicode;
        while ( i-- )
            *cp++ = *str++;
        
        _isUnicodeValid = 1;
    }
    else if (!_isUnicodeValid)
        QSTRING_FAILURE("invalid character cache");

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

QString QString::number(ulong n)
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

void QString::setBufferFromCFString(CFStringRef cfs)
{
    CFIndex size = CFStringGetLength(cfs);
    UniChar fixedSizeBuffer[1024];
    UniChar *buffer;
    if (size > (CFIndex)(sizeof(fixedSizeBuffer) / sizeof(UniChar))) {
        buffer = (UniChar *)malloc(size * sizeof(UniChar));
    } else {
        buffer = fixedSizeBuffer;
    }
    CFStringGetCharacters(cfs, CFRangeMake (0, size), buffer);
    setUnicode((const QChar *)buffer, (uint)size);
    if (buffer != fixedSizeBuffer) {
        free(buffer);
    }
}

// This function is used by the decoder. It will be replaced with TEC eventually.
QString QString::fromStringWithEncoding(const char *chs, int len, CFStringEncoding encoding)
{
    QString qs;
    
    if (chs && len > 0) {
        CFStringRef s = CFStringCreateWithBytes (kCFAllocatorDefault, (const UInt8 *)chs, len, encoding, false);
        if (s) {
            qs.setBufferFromCFString(s);
            CFRelease(s);
        }
    }
    
    return qs;
}

QString QString::fromCFString(CFStringRef cfs)
{
    QString qs;
    qs.setBufferFromCFString(cfs);
    return qs;
}

QString QString::fromNSString(NSString *nss)
{
    QString qs;
    qs.setBufferFromCFString((CFStringRef)nss);
    return qs;
}

CFStringRef QString::getCFString() const
{
    return (CFStringRef)getNSString();
}

NSString *QString::getNSString() const
{
    if (dataHandle[0]->_isUnicodeValid) {
        return [NSString stringWithCharacters:(const unichar *)unicode() length:dataHandle[0]->_length];
    }
    
    if (dataHandle[0]->_isAsciiValid) {
        return [(NSString *)CFStringCreateWithCString(kCFAllocatorDefault, ascii(), kCFStringEncodingISOLatin1) autorelease];
    }
    
    QSTRING_FAILURE("invalid character cache");
    return nil;
}

const QString QString::null;

QString::~QString()
{
    bool needToFreeHandle = false;
    QStringData *oldData = (*dataHandle);
    QStringData **oldHandle = dataHandle;
    
    KWQ_ASSERT(oldHandle);
    KWQ_ASSERT(oldData->refCount != 0);

    // Only free the handle if no other string has a reference to the
    // data.  The handle will be freed by the string that has the
    // last reference to data.
    if (dataHandle != makeSharedNullHandle() && oldData->refCount == 1)
        needToFreeHandle = true;

    // Copy our internal data if necessary, other strings still need it.
    detachInternal();
    
    // Remove our reference.  This should always be the last reference
    // if *dataHandle points to our internal QStringData.
    oldData->deref();

    KWQ_ASSERT(oldData != &internalData || oldData->refCount == 0);
    
    if (needToFreeHandle)
        freeHandle (oldHandle);

    dataHandle = 0;
}


QString::QString()
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    internalData.deref();
    dataHandle = makeSharedNullHandle();
    dataHandle[0]->ref();
}


// Careful, just used by QConstString
QString::QString(QStringData *constData, bool /*dummy*/) 
{
    internalData.deref();
    dataHandle = (QStringData **)allocateHandle();
    *dataHandle = constData;
    
    // The QConstString constructor allocated the QStringData.
    constData->_isHeapAllocated = 1;
}


QString::QString(QChar qc)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    dataHandle = (QStringData **)allocateHandle();

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

QString::QString(const QByteArray &qba)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    dataHandle = (QStringData **)allocateHandle();

    // Copy data
    *dataHandle = &internalData;
    internalData.initialize(qba.data(),qba.size());
}

QString::QString(const QChar *unicode, uint length)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    if ( !unicode && !length ) {
        internalData.deref();
        dataHandle = makeSharedNullHandle();
	dataHandle[0]->ref();
    } else {
        dataHandle = (QStringData **)allocateHandle();

        // Copy the QChar *
        *dataHandle = &internalData;
	internalData.initialize(unicode, length);
    }
}

QString::QString(const char *chs)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    dataHandle = (QStringData **)allocateHandle();
    *dataHandle = &internalData;

    if (chs)
        internalData.initialize(chs,strlen(chs));
    else
        internalData.initialize();
}

QString::QString(const char *chs, int len)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    dataHandle = (QStringData **)allocateHandle();
    *dataHandle = &internalData;
    internalData.initialize(chs,len);
}

QString::QString(const QString &qs) : dataHandle(qs.dataHandle)
{
#ifdef QSTRING_DEBUG_ALLOCATIONS
    countInstance (&dataHandle);
#endif
    internalData.deref();
    dataHandle[0]->ref();
}

QString &QString::operator=(const QString &qs)
{
    bool needToFreeHandle = false;
    
    if (this == &qs)
        return *this;

    // free our handle if it isn't the shared
    // null handle, and if no-one else is using
    // it.
    if (dataHandle != makeSharedNullHandle() && dataHandle[0]->refCount == 1)
        needToFreeHandle = true;
    
    qs.data()->ref();
    deref();
    
    if (needToFreeHandle)
        freeHandle (dataHandle);
        
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

inline QChar QString::at(uint i) const
{
    QStringData *thisData = (*dataHandle);
    
    if (i >= thisData->_length)
        return QChar::null;
        
    if (thisData->_isAsciiValid){
        if (thisData->_isAsciiInternal)
            return QChar(thisData->_internalBuffer[i]);
        else
            return QChar(thisData->_ascii[i]);
    }
    else if (thisData->_isUnicodeValid){
        if (thisData->_isUnicodeInternal)
            return ((QChar *)thisData->_internalBuffer)[i];
        else
            return thisData->_unicode[i];
    }
    else
        QSTRING_FAILURE("invalid character cache");
        
    return QChar::null;
}

const QChar *QString::unicode() const
{
    return dataHandle[0]->unicode();
}

int QString::compare( const QString& s ) const
{
    if (dataHandle[0]->_isAsciiValid && s.dataHandle[0]->_isAsciiValid)
        return strcmp (ascii(), s.ascii());
    return ucstrcmp(*this,s);
}

bool QString::startsWith( const QString& s ) const
{
    if (dataHandle[0]->_isAsciiValid){
        const char *asc = ascii();
        
        for ( int i =0; i < (int) s.data()->_length; i++ ) {
            if ( i >= (int) dataHandle[0]->_length || asc[i] != s[i] )
                return FALSE;
        }
    }
    else if (dataHandle[0]->_isUnicodeValid){
        const QChar *uni = unicode();
        
        for ( int i =0; i < (int) s.data()->_length; i++ ) {
            if ( i >= (int) dataHandle[0]->_length || uni[i] != s[i] )
                return FALSE;
        }
    }
    else
        QSTRING_FAILURE("invalid character cache");
        
    return TRUE;
}

bool QString::endsWith( const QString& s ) const
{
    const QChar *uni = unicode();

    if (dataHandle[0]->_length < s.data()->_length)
        return FALSE;
        
    for ( int i = dataHandle[0]->_length - s.data()->_length; i < (int) s.data()->_length; i++ ) {
	if ( uni[i] != s[i] )
	    return FALSE;
    }
    return TRUE;
}

const char *QString::latin1() const
{
    return dataHandle[0]->ascii();
}

QCString QString::utf8() const
{
    uint len = dataHandle[0]->_length;
    if (len == 0) {
        return QCString();
    }
    CFStringRef s = getCFString();
    CFIndex utf8Size;
    CFStringGetBytes(s, CFRangeMake(0, len), kCFStringEncodingUTF8, '?', false, 0, 0, &utf8Size);
    QCString qcs(utf8Size + 1);
    CFStringGetCString(s, qcs.data(), utf8Size + 1, kCFStringEncodingUTF8);
    return qcs;
}

QCString QString::local8Bit() const
{
    return utf8();
}

bool QString::isNull() const
{
    return dataHandle[0]->_length == 0;
}

int QString::find(QChar qc, int index) const
{
    if (IS_ASCII_QCHAR(qc) && dataHandle[0]->_isAsciiValid)
        return find((char)qc, index);
    else if (dataHandle[0]->_isUnicodeValid)
        return find(QString(qc), index, TRUE);
    else
        QSTRING_FAILURE("invalid character cache");

    // Should never get here.  Needed for compiler.
    return -1;
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
        return find(QChar(ch), index, TRUE);
    else
        QSTRING_FAILURE("invalid character cache");

    return -1;
}

int QString::find(const QString &str, int index, bool caseSensitive) const
{
    // FIXME, use the first character algorithm
    /*
      We use some weird hashing for efficiency's sake.  Instead of
      comparing strings, we compare the hash value of str with that of
      a part of this QString.  Only if that matches, we call ucstrncmp
      or ucstrnicmp.

      The hash value of a string is the sum of the cells of its
      QChars.
    */
    if ( index < 0 )
	index += dataHandle[0]->_length;
    int lstr = str.data()->_length;
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
	    hthis += uthis[i].cell();
	    hstr += ustr[i].cell();
	}
	i = 0;
	while ( TRUE ) {
	    if ( hthis == hstr && ucstrncmp(uthis + i, ustr, lstr) == 0 )
		return index + i;
	    if ( i == delta )
		return -1;
	    hthis += uthis[i + lstr].cell();
	    hthis -= uthis[i].cell();
	    i++;
	}
    } else {
	for ( i = 0; i < lstr; i++ ) {
	    hthis += uthis[i].lower().cell();
	    hstr += ustr[i].lower().cell();
	}
	i = 0;
	while ( TRUE ) {
	    if ( hthis == hstr && ucstrnicmp(uthis + i, ustr, lstr) == 0 )
		return index + i;
	    if ( i == delta )
		return -1;
	    hthis += uthis[i + lstr].lower().cell();
	    hthis -= uthis[i].lower().cell();
	    i++;
	}
    }
    
    // Should never get here.
    return -1;
}


static inline bool compareToLatinCharacter (UniChar c1, UniChar c2, bool caseSensitive)
{
    if (!caseSensitive){
        if (c2 >= 'a' && c2 <= 'z'){
            if (c1 == c2 || c1 == c2 - caseDelta)
                return true;
        }
        else if (c2 >= 'A' && c2 <= 'Z'){
            if (c1 == c2 || c1 == c2 + caseDelta)
                return true;
        }
        else if (c1 == c2)
            return true;
    }
    else if (c1 == c2)
        return true;
    return false;
}


// This function should be as fast as possible, every little bit helps.
// Our usage patterns are typically small strings.  In time trials
// this simplistic algorithm is much faster than Boyer-Moore or hash
// based alrogithms.
int QString::find(const char *chs, int index, bool caseSensitive) const
{
    if (dataHandle[0]->_isAsciiValid){
        char *ptr = dataHandle[0]->ascii();
        
        if (chs) {
            int len = dataHandle[0]->_length;
            
            ptr += index;
            
            if (len && (index >= 0) && (index < len)) {
                QChar firstC, c1, c2;
                const char *_chs;
                int remaining = len - index, found = -1;
                int compareToLength = strlen(chs);
                            
                _chs = chs + 1;
                firstC = (QChar)(*chs);
                while (remaining >= compareToLength){
                    if (compareToLatinCharacter((UniChar)*ptr++,firstC,caseSensitive)){
                        char *compareTo = ptr;
                        
                        found = len - remaining;
                        while ( (c2 = (UniChar)(*_chs++)) ){
                            c1 = (UniChar)(*compareTo++);
                            if (compareToLatinCharacter(c1, c2,caseSensitive))
                                continue;
                            break;
                        }
                        if (c2 == (QChar)0)
                            return found;
                        _chs = chs + 1;
                    }
                    remaining--;
                }
            }
        }
    }
    else if (dataHandle[0]->_isUnicodeValid){
        QChar *ptr = (QChar *)dataHandle[0]->unicode();

        if (chs) {
            int len = dataHandle[0]->_length;

            ptr += index;
            if (len && (index >= 0) && (index < len)) {
                QChar firstC, c1, c2;
                const char *_chs;
                int remaining = len - index, found = -1;
                int compareToLength = strlen(chs);
                            
                _chs = chs + 1;
                firstC = (QChar)(*chs);
                while (remaining >= compareToLength){
                    if (compareToLatinCharacter((UniChar)*ptr++,firstC,caseSensitive)){
                        QChar *compareTo = ptr;
                        
                        found = len - remaining;
                        while ( (c2 = (UniChar)(*_chs++)) ){
                            c1 = (UniChar)(*compareTo++);
                            if (compareToLatinCharacter(c1, c2,caseSensitive))
                                continue;
                            break;
                        }
                        if (c2 == (QChar)0)
                            return found;
                        _chs = chs + 1;
                    }
                    remaining--;
                }
            }
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
        QSTRING_FAILURE("invalid character cache");

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

    int lstr = str.data()->_length;
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
	while ( TRUE ) {
	    if ( hthis == hstr && ucstrncmp(uthis + i, ustr, lstr) == 0 )
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
	while ( TRUE ) {
	    if ( hthis == hstr && ucstrnicmp(uthis + i, ustr, lstr) == 0 )
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


#ifdef DEBUG_CONTAINS_COUNTER
static int containsCount = 0;
#endif

int QString::contains( QChar c, bool cs ) const
{
    int count = 0;
    
    if (dataHandle[0]->_isAsciiValid){
        if (!IS_ASCII_QCHAR(c))
            return 0;
            
        char tc, ac = (char)c;
        const char *cPtr = ascii();
        if ( !cPtr )
            return 0;
        int n = dataHandle[0]->_length;
        if ( cs ) {					// case sensitive
            while ( n-- )
                if ( *cPtr++ == ac )
                    count++;
        } else {					// case insensitive
            ac = (ac >= 'A' && ac <= 'Z') ? ac + caseDelta : ac;
            while ( n-- ) {
                tc = *cPtr++;
                tc =  (tc >= 'A' && tc <= 'Z') ? tc + caseDelta : tc;
                if ( tc == ac )
                    count++;
            }
        }
    }
    else if (dataHandle[0]->_isUnicodeValid){
        const QChar *uc = unicode();
        if ( !uc )
            return 0;
        int n = dataHandle[0]->_length;
        if ( cs ) {					// case sensitive
            while ( n-- )
                if ( *uc++ == c )
                    count++;
        } else {					// case insensitive
            c = c.lower();
            while ( n-- ) {
                if ( uc->lower() == c )
                    count++;
                uc++;
            }
        }
    } 
    else
        QSTRING_FAILURE("invalid character cache");
    return count;
}

int QString::contains(char ch) const
{
    return contains (QChar(ch),true);
}

int QString::contains(const char *str, bool caseSensitive) const
{
    if ( !str )
        return 0;

    if (dataHandle[0]->_isAsciiValid){
        int count = 0;
        const char *uc = ascii();
        int n = dataHandle[0]->_length;
        int toLen = strlen(str);
        
        while ( n-- ) {
            if ( caseSensitive ) {
                if ( strncmp( uc, str, toLen ) == 0 )
                    count++;
            } else {
                if ( strncasecmp(uc, str, toLen) == 0 )
                    count++;
            }
            uc++;
        }
        return count;
    }
    else if (dataHandle[0]->_isUnicodeValid)
        return contains(QString(str),caseSensitive);
    else
        QSTRING_FAILURE("invalid character cache");

    return 0;
}

int QString::contains(const QString &str, bool caseSensitive) const
{
    int count = 0;
    const QChar *uc = unicode();
    if ( !str )
	return 0;
    int len = str.data()->_length;
    int n = dataHandle[0]->_length;
    while ( n-- ) {				// counts overlapping strings
	// ### Doesn't account for length of this - searches over "end"
	if ( caseSensitive ) {
	    if ( ucstrncmp( uc, str.unicode(), len ) == 0 )
		count++;
	} else {
	    if ( ucstrnicmp(uc, str.unicode(), len) == 0 )
		count++;
	}
	uc++;
    }
    return count;
}

short QString::toShort(bool *ok, int base) const
{
    long v = toLong( ok, base );
    if ( ok && *ok && (v < -32768 || v > 32767) ) {
	*ok = FALSE;
	v = 0;
    }
    return (short)v;
}

ushort QString::toUShort(bool *ok, int base) const
{
    ulong v = toULong( ok, base );
    if ( ok && *ok && (v > 65535) ) {
	*ok = FALSE;
	v = 0;
    }
    return (ushort)v;
}

int QString::toInt(bool *ok, int base) const
{
    return (int)toLong( ok, base );
}

uint QString::toUInt(bool *ok, int base) const
{
    return (uint)toULong( ok, base );
}

long QString::toLong(bool *ok, int base) const
{
    const QChar *p = unicode();
    long val=0;
    int l = dataHandle[0]->_length;
    const long max_mult = INT_MAX / base;
    bool is_ok = FALSE;
    int neg = 0;
    if ( !p )
	goto bye;
    while ( l && p->isSpace() )			// skip leading space
	l--,p++;
    if ( l && *p == '-' ) {
	l--;
	p++;
	neg = 1;
    } else if ( *p == '+' ) {
	l--;
	p++;
    }

    // NOTE: toULong() code is similar
    if ( !l || !ok_in_base(*p,base) )
	goto bye;
    while ( l && ok_in_base(*p,base) ) {
	l--;
	int dv;
	if ( p->isDigit() ) {
	    dv = p->digitValue();
	} else {
	    if ( *p >= 'a' && *p <= 'z' )
		dv = *p - 'a' + 10;
	    else
		dv = *p - 'A' + 10;
	}
	if ( val > max_mult || (val == max_mult && dv > (INT_MAX%base)+neg) )
	    goto bye;
	val = base*val + dv;
	p++;
    }
    if ( neg )
	val = -val;
    while ( l && p->isSpace() )			// skip trailing space
	l--,p++;
    if ( !l )
	is_ok = TRUE;
bye:
    if ( ok )
	*ok = is_ok;
    return is_ok ? val : 0;
}

ulong QString::toULong(bool *ok, int base) const
{
    const QChar *p = unicode();
    ulong val=0;
    int l = dataHandle[0]->_length;
    const ulong max_mult = UINT_MAX / base;
    bool is_ok = FALSE;
    if ( !p )
	goto bye;
    while ( l && p->isSpace() )			// skip leading space
	l--,p++;
    if ( *p == '+' )
	l--,p++;

    // NOTE: toLong() code is similar
    if ( !l || !ok_in_base(*p,base) )
	goto bye;
    while ( l && ok_in_base(*p,base) ) {
	l--;
	uint dv;
	if ( p->isDigit() ) {
	    dv = p->digitValue();
	} else {
	    if ( *p >= 'a' && *p <= 'z' )
		dv = *p - 'a' + 10;
	    else
		dv = *p - 'A' + 10;
	}
	if ( val > max_mult || (val == max_mult && dv > (UINT_MAX%base)) )
	    goto bye;
	val = base*val + dv;
	p++;
    }

    while ( l && p->isSpace() )			// skip trailing space
	l--,p++;
    if ( !l )
	is_ok = TRUE;
bye:
    if ( ok )
	*ok = is_ok;
    return is_ok ? val : 0;
}

double QString::toDouble(bool *ok) const
{
    char *end;

    QCString a = latin1();

    double val = strtod( a.data() ? a.data() : "", &end );
    if ( ok )
	*ok = ( a && *a && ( end == 0 || *end == '\0' ) );
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
	qWarning( "QString::arg(): Argument missing: %s, %s",
		  latin1(), a.latin1() );
	// Make sure the text at least appears SOMEWHERE
	r += ' ';
	pos = r.data()->_length;
	len = 0;
    }

    r.replace( pos, len, a );
    if ( fieldwidth < 0 ) {
	QString s;
	while ( (uint)-fieldwidth > a.data()->_length ) {
	    s += ' ';
	    fieldwidth++;
	}
	r.insert( pos + a.data()->_length, s );
    } else if ( fieldwidth ) {
	QString s;
	while ( (uint)fieldwidth > a.data()->_length ) {
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

QString QString::arg(ushort replacement, int width) const
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

QString QString::arg(ulong replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::arg(double replacement, int width) const
{
    return arg(number(replacement), width);
}

QString QString::left(uint len) const
{
    if ( isEmpty() ) {
	return QString();
    } else if ( len == 0 ) {			// ## just for 1.x compat:
	return QString::fromLatin1("");
    } else if ( len > dataHandle[0]->_length ) {
	return *this;
    } else {
	QString s( unicode(), len );
	return s;
    }
}

QString QString::right(uint len) const
{
    if ( isEmpty() ) {
	return QString();
    } else if ( len == 0 ) {			// ## just for 1.x compat:
	return QString::fromLatin1("");
    } else {
	uint l = dataHandle[0]->_length;
	if ( len > l )
	    len = l;
	QString s( unicode()+(l-len), len );
	return s;
    }
}

QString QString::mid(uint index, uint len) const
{
    uint slen = dataHandle[0]->_length;
    if ( isEmpty() || index >= slen ) {
	return QString();
    } else if ( len == 0 ) {			// ## just for 1.x compat:
	return QString::fromLatin1("");
    } else {
	if ( len > slen-index )
	    len = slen - index;
	if ( index == 0 && len == dataHandle[0]->_length )
	    return *this;
	register const QChar *p = unicode()+index;
	QString s( p, len );
	return s;
    }
}

QString QString::copy() const
{
    // does not need to be a deep copy
    return QString(*this);
}

QString QString::lower() const
{
    QString s(*this);
    int l = dataHandle[0]->_length;
    if ( l ) {
	s.detach();
        if (s.data()->_isAsciiValid){
            char *p= (char *)s.ascii();
            if ( p ) {
                while ( l-- ) {
                    *p = (*p >= 'A' && *p <= 'Z') ? (*p + caseDelta) : *p;
                    p++;
                }
            }
        }
        else if (s.data()->_isUnicodeValid){
            QChar *p= (QChar *)s.unicode();
            if ( p ) {
                while ( l-- ) {
                    if (IS_ASCII_QCHAR(*p)){
                        *p = (*p >= (QChar)'A' && *p <= (QChar)'Z') ? (QChar)(*p + caseDelta) : *p;
                    }
                    else
                        *p = p->lower();
                    p++;
                }
            }
        }
        else
            QSTRING_FAILURE("invalid character cache");
    }
    return s;
}

QString QString::stripWhiteSpace() const
{
    if ( isEmpty() )				// nothing to do
	return *this;
    if ( !at(0).isSpace() && !at(dataHandle[0]->_length-1).isSpace() )
	return *this;

    int start = 0;
    int end = dataHandle[0]->_length - 1;

    QString result = fromLatin1("");
    while ( start<=end && at(start).isSpace() )	// skip white space from start
        start++;
    if ( start > end ) {			// only white space
        return result;
    }
    while ( end && at(end).isSpace() )		// skip white space from end
        end--;
    int l = end - start + 1;
    
    if (dataHandle[0]->_isAsciiValid){
        result.setLength( l );
        if ( l )
            memcpy( (char *)result.data()->ascii(), &ascii()[start], l );
    }
    else if (dataHandle[0]->_isUnicodeValid){
        result.forceUnicode();
        result.setLength( l );
        if ( l )
            memcpy( (QChar *)result.data()->unicode(), &unicode()[start], sizeof(QChar)*l );
    }
    else
        QSTRING_FAILURE("invalid character cache");
    return result;
}

QString QString::simplifyWhiteSpace() const
{
    if ( isEmpty() )				// nothing to do
	return *this;
    
    QString result;

    if (dataHandle[0]->_isAsciiValid){
        result.setLength( dataHandle[0]->_length );
        const char *from = ascii();
        const char *fromend = from + dataHandle[0]->_length;
        int outc=0;
        
        char *to = (char *)result.ascii();
        while ( TRUE ) {
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
        result.forceUnicode();
        result.setLength( dataHandle[0]->_length );
        const QChar *from = unicode();
        const QChar *fromend = from + dataHandle[0]->_length;
        int outc=0;
        
        QChar *to = (QChar *)result.unicode();
        while ( TRUE ) {
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
        QSTRING_FAILURE("invalid character cache");
    
    return result;
}

void QString::deref()
{
    dataHandle[0]->deref();
}


QString &QString::setUnicode(const QChar *uni, uint len)
{
    detach();
    
    bool needToFreeHandle = false;
    
    // free our handle if it isn't the shared
    // null handle, and if no-one else is using
    // it.
    if (dataHandle != makeSharedNullHandle() && dataHandle[0]->refCount == 1)
        needToFreeHandle = true;
        
    if ( len == 0 ) {
        deref();
        if (needToFreeHandle)
            freeHandle (dataHandle);
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else if (len > dataHandle[0]->_maxUnicode || dataHandle[0]->refCount != 1 || !dataHandle[0]->_isUnicodeValid) {
        deref();
        if (needToFreeHandle)
            freeHandle (dataHandle);
        dataHandle = (QStringData **)allocateHandle();
	*dataHandle = new QStringData(uni, len);
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

    detach();
    
    bool needToFreeHandle = false;
    
    // free our handle if it isn't the shared
    // null handle, and if no-one else is using
    // it.
    if (dataHandle != makeSharedNullHandle() && dataHandle[0]->refCount == 1)
        needToFreeHandle = true;
        
    if (len == 0) {
        deref();
        if (needToFreeHandle)
            freeHandle (dataHandle);
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else if (len+1 > (int)dataHandle[0]->_maxAscii || dataHandle[0]->refCount != 1 || !dataHandle[0]->_isAsciiValid) {
        deref();
        if (needToFreeHandle)
            freeHandle (dataHandle);
        dataHandle = (QStringData **)allocateHandle();
        *dataHandle = new QStringData(str,len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        strcpy( (char *)ascii(), str );
        dataHandle[0]->_length = len;
        dataHandle[0]->_isUnicodeValid = 0;
    }
    return *this;
}

QString &QString::setNum(short n)
{
    return sprintf("%d", n);
}

QString &QString::setNum(ushort n)
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

QString &QString::setNum(ulong n)
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
    size_t len = vsnprintf(&ch, 1, format, args);
    
    // Handle the empty string case to simplify the code below.
    if (len == 0) {
        setUnicode(0, 0);
        return *this;
    }
    
    // Arrange for storage for the resulting string.
    detach();
    if (len >= dataHandle[0]->_maxAscii || dataHandle[0]->refCount != 1 || !dataHandle[0]->_isAsciiValid) {
        bool needToFreeHandle = dataHandle != makeSharedNullHandle() && dataHandle[0]->refCount == 1;
        deref();
        if (needToFreeHandle)
            freeHandle(dataHandle);
        dataHandle = (QStringData **)allocateHandle();
        *dataHandle = new QStringData((char *)0, len);
        dataHandle[0]->_isHeapAllocated = 1;
    } else {
        dataHandle[0]->_length = len;
        dataHandle[0]->_isUnicodeValid = 0;
    }

    // Now do the formatting again, guaranteed to fit.
    vsprintf((char *)ascii(), format, args);

    return *this;
}

QString &QString::prepend(const QString &qs)
{
    return insert(0, qs);
}

QString &QString::append(const QString &qs)
{
    return insert(dataHandle[0]->_length, qs);
}


QString &QString::insert(uint index, const char *insertChars, uint insertLength)
{
    detach();
    
    if (insertLength == 0)
        return *this;
        
    if (dataHandle[0]->_isAsciiValid){
        uint originalLength = dataHandle[0]->_length;
        char *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        targetChars = (char *)ascii();
        
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
        QSTRING_FAILURE("invalid character cache");
    
    return *this;
}


QString &QString::insert(uint index, const QString &qs)
{
    detach();
    
    if (qs.data()->_length == 0)
        return *this;
        
#ifdef QSTRING_DEBUG_UNICODE
    forceUnicode();
#endif
    if (dataHandle[0]->_isAsciiValid && qs.data()->_isAsciiValid){
        insert (index, qs.ascii(), qs.length());
    }
    else {
        uint insertLength = qs.data()->_length;
        uint originalLength = dataHandle[0]->_length;
        QChar *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + insertLength);
        forceUnicode();
        targetChars = (QChar *)unicode();
        
        // Move tail to make space for inserted characters.
        memmove (targetChars+(index+insertLength), targetChars+index, (originalLength-index)*sizeof(QChar));

        // Insert characters.
        if (qs.data()->_isAsciiValid){
            uint i = insertLength;
            QChar *target = targetChars+index;
            char *a = (char *)qs.ascii();
            
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


QString &QString::insert(uint index, QChar qc)
{
    detach();
    
    if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(qc)){
        uint originalLength = dataHandle[0]->_length;
        char insertChar = (char)qc;
        char *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = (char *)ascii();
        
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
        forceUnicode();
        targetChars = (QChar *)unicode();
        
        // Move tail to make space for inserted character.
        memmove (targetChars+(index+1), targetChars+index, (originalLength-index)*sizeof(QChar));

        targetChars[index] = qc;
    }
    
    return *this;
}


QString &QString::insert(uint index, char ch)
{
    detach();
    
    if (dataHandle[0]->_isAsciiValid){
        uint originalLength = dataHandle[0]->_length;
        char *targetChars;
        
        // Ensure that we have enough space.
        setLength (originalLength + 1);
        targetChars = (char *)ascii();
        
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
        QSTRING_FAILURE("invalid character cache");
    
    return *this;
}


void QString::detachInternal()
{
    QStringData *oldData = (*dataHandle), *newData = 0;
    
    if (oldData->refCount > 1 && oldData == &internalData){
        if (oldData->_isAsciiValid)
            newData = new QStringData (oldData->ascii(), oldData->_length);
        else if (oldData->_isUnicodeValid){
            // No need to copy the allocated unicode bytes.
            if (!oldData->_isUnicodeInternal){
                newData = new QStringData (oldData->unicode(), oldData->_length, oldData->_maxUnicode);
                oldData->_unicode = 0;
                oldData->_isUnicodeValid = 0;
            }
            else
                newData = new QStringData (oldData->unicode(), oldData->_length);
        }
        else
            QSTRING_FAILURE("invalid character cache");
        newData->_isHeapAllocated = 1;
        newData->refCount = oldData->refCount - 1;
        *dataHandle = newData;
        
        oldData->refCount -= (oldData->refCount - 1);
    }
}

// Copy QStringData if necessary.  Must be called before
// the string data is mutated.
void QString::detach()
{
    if (dataHandle[0]->refCount == 1 && dataHandle != shared_null_handle)
        return;

#ifdef QSTRING_DEBUG_ALLOCATIONS
    stringDataDetachments++;
#endif
    QStringData *oldData = (*dataHandle), *newData = 0;
    
    // Copy data for this string so we can safely mutate it,
    // and put it in a new handle.
    if (oldData->_isAsciiValid)
        newData = new QStringData (oldData->ascii(), oldData->_length);
    else if (oldData->_isUnicodeValid)
        newData = new QStringData (oldData->unicode(), oldData->_length);
    else if (oldData == shared_null){
        newData = new QStringData ();
    }
    else
        QSTRING_FAILURE("invalid character cache");

    // We must copy our internal data so other strings
    // can still safely reference it.
    detachInternal();
    
    newData->_isHeapAllocated = 1;
    dataHandle = (QStringData **)allocateHandle();
    *dataHandle = newData;
    
    // Release the old data.
    oldData->deref();
}

QString &QString::remove(uint index, uint len)
{
    uint olen = dataHandle[0]->_length;
    if ( index >= olen  ) {
        // range problems
    } else if ( index + len >= olen ) {  // index ok
        setLength( index );
    } else if ( len != 0 ) {
	detach();
        
#ifdef QSTRING_DEBUG_UNICODE
    forceUnicode();
#endif
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
            QSTRING_FAILURE("invalid character cache");
    }
    return *this;
}

QString &QString::replace( uint index, uint len, const QString &str )
{
    // No need to detach here, detach will happen if necessary
    // in remove() or insert().
    
    if (str.data()->_length == 0)
        return *this;
        
    remove( index, len );
    insert( index, str );

    return *this;
}

QString &QString::replace(const QRegExp &qre, const QString &str)
{
    if ( isEmpty() )
	return *this;
    int index = 0;
    int slen  = str.data()->_length;
    int len;
    while ( index < (int)dataHandle[0]->_length ) {
	index = qre.match( *this, index, &len, FALSE );
	if ( index >= 0 ) {
	    replace( index, len, str );
	    index += slen;
	    if ( !len )
		break;	// Avoid infinite loop on 0-length matches, e.g. [a-z]*
	}
	else
	    break;
    }
    return *this;
}



void QString::forceUnicode()
{
    detach();
    
    unicode();

    dataHandle[0]->_isAsciiValid = 0;
}


// Increase buffer size if necessary.  Newly allocated
// bytes will contain garbage.
void QString::setLength( uint newLen )
{
    detach();
    
    // If we going to change the length, we'll need our 
    // own data.
    if (dataHandle == makeSharedNullHandle()){
        deref();
        dataHandle = (QStringData **)allocateHandle();
        *dataHandle = new QStringData();
        dataHandle[0]->_isHeapAllocated = 1;
    }
    
#ifdef QSTRING_DEBUG_UNICODE
    forceUnicode();
#endif
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
        QSTRING_FAILURE("invalid character cache");

    dataHandle[0]->_length = newLen;
}


void QString::truncate(uint newLen)
{
    if ( newLen < dataHandle[0]->_length )
	setLength( newLen );
}

void QString::fill(QChar qc, int len)
{
    detach();
    
#ifdef QSTRING_DEBUG_UNICODE
    forceUnicode();
#endif
    // len == -1 means fill to string length.
    if ( len < 0 )
	len = dataHandle[0]->_length;
        
    if ( len == 0 ) {
        deref();
        dataHandle = makeSharedNullHandle();
        dataHandle[0]->ref();
    } else {
        if (dataHandle[0]->_isAsciiValid && IS_ASCII_QCHAR(qc)){
            setLength(len);
            char *nd = (char *)ascii();
            while (len--) 
                *nd++ = (char)qc;
            dataHandle[0]->_isUnicodeValid = 0;
        }
        else {
            forceUnicode();
            setLength(len);
            QChar *nd = (QChar *)unicode();
            while (len--) 
                *nd++ = qc;
        }
    }
}

void QString::compose()
{
    // FIXME: unimplemented because we don't do ligatures yet
    _logNotYetImplemented();
}

QString QString::visual()
{
    // FIXME: unimplemented because we don't do BIDI yet
    _logNotYetImplemented();
    return QString(*this);
}

QString &QString::operator+=(const QString &qs)
{
    detach();

    if (dataHandle[0]->_isUnicodeValid && dataHandle[0]->_length + qs.data()->_length < dataHandle[0]->_maxUnicode){
        uint i = qs.data()->_length;
        QChar *tp = &dataHandle[0]->_unicode[dataHandle[0]->_length];
        if (qs.data()->_isAsciiValid){
            char *fp = (char *)qs.ascii();
            while (i--)
                *tp++ = *fp++;
        }
        else if(qs.data()->_isUnicodeValid){
            QChar *fp = (QChar *)qs.unicode();
            while (i--)
                *tp++ = *fp++;
        }
        else 
            QSTRING_FAILURE("invalid character cache");
        dataHandle[0]->_length += qs.data()->_length;
        dataHandle[0]->_isAsciiValid = 0;
        return *this;
    }
    else if (dataHandle[0]->_isAsciiValid && qs.data()->_isAsciiValid && dataHandle[0]->_length + qs.data()->_length < dataHandle[0]->_maxAscii){
        uint i = qs.data()->_length;
        char *tp = &dataHandle[0]->_ascii[dataHandle[0]->_length];
        char *fp = (char *)qs.ascii();
        while (i--)
            *tp++ = *fp++;
        *tp = 0;
        dataHandle[0]->_length += qs.data()->_length;
        dataHandle[0]->_isUnicodeValid = 0;
        return *this;
    }
    return insert(dataHandle[0]->_length, qs);
}

QString &QString::operator+=(QChar qc)
{
    detach();
    
    QStringData *thisData = *dataHandle;
    if (thisData->_isUnicodeValid && thisData->_length + 1 < thisData->_maxUnicode){
        thisData->_unicode[data()->_length] = qc;
        thisData->_length++;
        thisData->_isAsciiValid = 0;
        return *this;
    }
    else if (thisData->_isAsciiValid && IS_ASCII_QCHAR(qc) && thisData->_length + 2 < thisData->_maxAscii){
        thisData->_ascii[data()->_length] = (char)qc;
        thisData->_length++;
        thisData->_ascii[data()->_length] = 0;
        thisData->_isUnicodeValid = 0;
        return *this;
    }
    return insert(thisData->_length, qc);
}

QString &QString::operator+=(char ch)
{
    detach();
    
    QStringData *thisData = *dataHandle;
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

bool operator==(const QString &s1, const QString &s2)
{
    if (s1.data()->_isAsciiValid && s2.data()->_isAsciiValid) {
        return strcmp(s1.ascii(), s2.ascii()) == 0;
    }
    return s1.data()->_length == s2.data()->_length
        && memcmp(s1.unicode(), s2.unicode(), s1.data()->_length * sizeof(QChar)) == 0;
}

bool operator==(const QString &s1, const char *chs)
{
    if (s1.data()->_isAsciiValid)
        return strcmp(s1.ascii(), chs) == 0;
    return s1 == QString(chs);
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
    QString(new QStringData((QChar *)unicode, length, length), true)
{
}

QConstString::~QConstString()
{
    if (dataHandle[0]->refCount > 1) {
        QChar *tp, *fp = (QChar *)unicode();
        if (dataHandle[0]->_length <= QS_INTERNAL_BUFFER_UCHARS){
            dataHandle[0]->_maxUnicode = QS_INTERNAL_BUFFER_UCHARS;
            tp = (QChar *)&dataHandle[0]->_internalBuffer[0];
            dataHandle[0]->_isUnicodeInternal = 1;
        }
        else {
            dataHandle[0]->_maxUnicode = ALLOC_QCHAR_GOOD_SIZE(dataHandle[0]->_length);
            tp = ALLOC_QCHAR( dataHandle[0]->_maxUnicode );
            dataHandle[0]->_isUnicodeInternal = 0;
        }
        memcpy( tp, fp, dataHandle[0]->_length*sizeof(QChar) );
        dataHandle[0]->_unicode = tp;
	dataHandle[0]->_isUnicodeValid = 1;
        dataHandle[0]->_isAsciiValid = 0;
    } else {
	dataHandle[0]->_unicode = 0;
    }
}

#define NODE_BLOCK_SIZE ((vm_page_size)/sizeof(HandleNode))

#define TO_NODE_OFFSET(ptr)   ((uint)(((uint)ptr - (uint)base)/sizeof(HandleNode)))
#define TO_NODE_ADDRESS(offset,base) ((HandleNode *)(offset*sizeof(HandleNode) + (uint)base))

struct HandlePageNode
{
    HandlePageNode *next;
    HandlePageNode *previous;
    void *nodes;
};

struct HandleNode {
    union {
        struct {
            uint next:16;
            uint previous:16;
        } internalNode;
        
        HandleNode *freeNodes;  // Always at block[0] in page.
        
        HandlePageNode *pageNode;	// Always at block[1] in page
        
        void *handle;
    } type;
};

static HandlePageNode *usedNodeAllocationPages = 0;
static HandlePageNode *freeNodeAllocationPages = 0;

static HandleNode *_initializeHandleNodeBlock(HandlePageNode *pageNode)
{
    uint i;
    HandleNode *block, *aNode;
    
    vm_allocate(mach_task_self(), (vm_address_t *)&block, vm_page_size, 1);
    //printf ("allocated block at 0%08x, page boundary 0x%08x\n", (unsigned int)block, (unsigned int)trunc_page((uint)block));
    
    for (i = 2; i < NODE_BLOCK_SIZE; i++){
        aNode = &block[i];
        if (i > 2) {
            aNode->type.internalNode.previous = i-1;
        }
        else {
            aNode->type.internalNode.previous = 0;
        }
        if (i != NODE_BLOCK_SIZE-1)
            aNode->type.internalNode.next = i+1;
        else
            aNode->type.internalNode.next = 0;
    }
    block[0].type.freeNodes = &block[NODE_BLOCK_SIZE-1];
    block[1].type.pageNode = pageNode;

    return block;
}

HandlePageNode *_allocatePageNode()
{
    HandlePageNode *node = (HandlePageNode *)malloc(sizeof(HandlePageNode));
    node->next = node->previous = 0;
    node->nodes = _initializeHandleNodeBlock(node);
    return node;
}

void _initializeHandleNodes()
{
    if (freeNodeAllocationPages == 0)
        freeNodeAllocationPages = _allocatePageNode();    
}

HandleNode *_allocateNode(HandlePageNode *pageNode)
{
    HandleNode *block = (HandleNode *)pageNode->nodes;
    HandleNode *freeNodes = block[0].type.freeNodes;
    HandleNode *allocated;
    
    // Check to see if we're out of nodes.
    if (freeNodes == 0) {
        QSTRING_FAILURE("out of nodes");
        return 0;
    }
    
    // Remove node from end of free list 
    allocated = freeNodes;
    if (allocated->type.internalNode.previous >= 2) {
        block[0].type.freeNodes = TO_NODE_ADDRESS(allocated->type.internalNode.previous, block);
        block[0].type.freeNodes->type.internalNode.next = 0;
    }
    else {
        // Used last noded on this page.
        block[0].type.freeNodes = 0;
        
        freeNodeAllocationPages = freeNodeAllocationPages->previous;

        pageNode->previous = usedNodeAllocationPages;
        pageNode->next = 0;
        if (usedNodeAllocationPages)
            usedNodeAllocationPages->next = pageNode;
        usedNodeAllocationPages = pageNode;        
    }
        
    return allocated;
}


void *allocateHandle()
{
    _initializeHandleNodes();
    
#ifdef QSTRING_DEBUG_ALLOCATIONS
    handleInstances++;
#endif

    return _allocateNode (freeNodeAllocationPages);
}


void freeHandle(void *_free)
{
    HandleNode *free = (HandleNode *)_free;
    HandleNode *base = (HandleNode *)trunc_page((uint)free);
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
    if (usedNodeAllocationPages == pageNode)
        usedNodeAllocationPages = usedNodeAllocationPages->previous;
    else if (freeNodeAllocationPages != pageNode){
        if (pageNode->previous)
            pageNode->previous->next = pageNode->next;
        if (pageNode->next)
            pageNode->next->previous = pageNode->previous;
    
        pageNode->previous = freeNodeAllocationPages;
        pageNode->next = 0;
        if (freeNodeAllocationPages)
            freeNodeAllocationPages->next = pageNode;
        freeNodeAllocationPages = pageNode;
    }
    
#ifdef QSTRING_DEBUG_ALLOCATIONS
    handleInstances--;
#endif
}
