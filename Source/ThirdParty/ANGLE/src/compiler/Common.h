//
// Copyright (c) 2002-2010 The ANGLE Project Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//

#ifndef _COMMON_INCLUDED_
#define _COMMON_INCLUDED_

#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "compiler/PoolAlloc.h"

// We need two pieces of information to report errors/warnings - string and
// line number. We encode these into a single int so that it can be easily
// incremented/decremented by lexer. The right SOURCE_LOC_LINE_SIZE bits store
// line number while the rest store the string number. Since the shaders are
// usually small, we should not run out of memory. SOURCE_LOC_LINE_SIZE
// can be increased to alleviate this issue.
typedef int TSourceLoc;
const unsigned int SOURCE_LOC_LINE_SIZE = 16;  // in bits.
const unsigned int SOURCE_LOC_LINE_MASK = (1 << SOURCE_LOC_LINE_SIZE) - 1;

inline TSourceLoc EncodeSourceLoc(int string, int line) {
    return (string << SOURCE_LOC_LINE_SIZE) | (line & SOURCE_LOC_LINE_MASK);
}

inline void DecodeSourceLoc(TSourceLoc loc, int* string, int* line) {
    if (string) *string = loc >> SOURCE_LOC_LINE_SIZE;
    if (line) *line = loc & SOURCE_LOC_LINE_MASK;
}

//
// Put POOL_ALLOCATOR_NEW_DELETE in base classes to make them use this scheme.
//
#define POOL_ALLOCATOR_NEW_DELETE(A)                                  \
    void* operator new(size_t s) { return (A).allocate(s); }          \
    void* operator new(size_t, void *_Where) { return (_Where);	}     \
    void operator delete(void*) { }                                   \
    void operator delete(void *, void *) { }                          \
    void* operator new[](size_t s) { return (A).allocate(s); }        \
    void* operator new[](size_t, void *_Where) { return (_Where);	} \
    void operator delete[](void*) { }                                 \
    void operator delete[](void *, void *) { }

//
// Pool version of string.
//
typedef pool_allocator<char> TStringAllocator;
typedef std::basic_string <char, std::char_traits<char>, TStringAllocator> TString;
typedef std::basic_ostringstream<char, std::char_traits<char>, TStringAllocator> TStringStream;
inline TString* NewPoolTString(const char* s)
{
	void* memory = GlobalPoolAllocator.allocate(sizeof(TString));
	return new(memory) TString(s);
}

//
// Persistent string memory.  Should only be used for strings that survive
// across compiles.
//
#define TPersistString std::string
#define TPersistStringStream std::ostringstream

//
// Pool allocator versions of vectors, lists, and maps
//
template <class T> class TVector : public std::vector<T, pool_allocator<T> > {
public:
    typedef typename std::vector<T, pool_allocator<T> >::size_type size_type;
    TVector() : std::vector<T, pool_allocator<T> >() {}
    TVector(const pool_allocator<T>& a) : std::vector<T, pool_allocator<T> >(a) {}
    TVector(size_type i): std::vector<T, pool_allocator<T> >(i) {}
};

template <class K, class D, class CMP = std::less<K> > 
class TMap : public std::map<K, D, CMP, pool_allocator<std::pair<const K, D> > > {
public:
    typedef pool_allocator<std::pair<const K, D> > tAllocator;

    TMap() : std::map<K, D, CMP, tAllocator>() {}
    // use correct two-stage name lookup supported in gcc 3.4 and above
    TMap(const tAllocator& a) : std::map<K, D, CMP, tAllocator>(std::map<K, D, CMP, tAllocator>::key_compare(), a) {}
};

#endif // _COMMON_INCLUDED_
