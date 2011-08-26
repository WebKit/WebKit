/*
 * Copyright (C) 2011 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */

#include "config.h"
#include "VTableSpectrum.h"

#include "JSObject.h"
#include "Structure.h"
#include <algorithm>
#include <stdio.h>
#include <wtf/Platform.h>
#include <wtf/Vector.h>

#if PLATFORM(MAC)
#include <dlfcn.h>
#endif

namespace JSC {

VTableSpectrum::VTableSpectrum()
{
}

VTableSpectrum::~VTableSpectrum()
{
}

void VTableSpectrum::countVPtr(void* vTablePointer)
{
    std::pair<HashMap<void*, unsigned long>::iterator, bool> result = m_map.add(vTablePointer, 1);
    if (!result.second)
        result.first->second++;
}

void VTableSpectrum::count(JSCell* cell)
{
    countVPtr(cell->vptr());
}

struct VTableAndCount {
    void* vtable;
    unsigned long count;
    
    VTableAndCount() { }
    
    VTableAndCount(void* vtable, unsigned long count)
        : vtable(vtable)
        , count(count)
    {
    }
    
    bool operator<(const VTableAndCount& other) const
    {
        if (count != other.count)
            return count < other.count;
        return vtable > other.vtable; // this results in lower-addressed vtables being printed first
    }
};

void VTableSpectrum::dump(FILE* output, const char* comment)
{
    fprintf(output, "%s:\n", comment);
    
    HashMap<void*, unsigned long>::iterator begin = m_map.begin();
    HashMap<void*, unsigned long>::iterator end = m_map.end();
    
    Vector<VTableAndCount, 0> list;
    
    for (HashMap<void*, unsigned long>::iterator iter = begin; iter != end; ++iter)
        list.append(VTableAndCount(iter->first, iter->second));
    
    std::sort(list.begin(), list.end());
    
    for (size_t index = list.size(); index-- > 0;) {
        VTableAndCount item = list.at(index);
#if PLATFORM(MAC)
        Dl_info info;
        if (dladdr(item.vtable, &info)) {
            char* findResult = strrchr(info.dli_fname, '/');
            const char* strippedFileName;
            
            if (findResult)
                strippedFileName = findResult + 1;
            else
                strippedFileName = info.dli_fname;
            
            fprintf(output, "    %s:%s(%p): %lu\n", strippedFileName, info.dli_sname, item.vtable, item.count);
            continue;
        }
#endif
        fprintf(output, "    %p: %lu\n", item.vtable, item.count);
    }
    
    fflush(output);
}

} // namespace JSC
