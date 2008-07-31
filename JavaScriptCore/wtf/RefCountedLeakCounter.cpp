/*
 *  Copyright (C) 2008 Apple Inc. All rights reserved.
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 *  You should have received a copy of the GNU Library General Public License
 *  along with this library; see the file COPYING.LIB.  If not, write to
 *  the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
 *  Boston, MA 02110-1301, USA.
 *
 */

#include "config.h"

#include "RefCountedLeakCounter.h"
#include "UnusedParam.h"

namespace WTF {

#ifndef NDEBUG
    static bool logLeakMessages = true;
#endif
    
    void setLogLeakMessages(bool _logLeakMessages )
    {
        UNUSED_PARAM(_logLeakMessages);
#ifndef NDEBUG
        logLeakMessages = _logLeakMessages;
#endif
    }
    
    
#ifndef NDEBUG
#define LOG_CHANNEL_PREFIX Log
    static WTFLogChannel LogRefCountedLeaks = { 0x00000000, "", WTFLogChannelOn };
#endif

    RefCountedLeakCounter::~RefCountedLeakCounter()
    {
#ifndef NDEBUG
        if (count && logLeakMessages)
            LOG(RefCountedLeaks, "LEAK: %u %s\n", count, description);
#endif
    }

    RefCountedLeakCounter::RefCountedLeakCounter(const char* desc)
    {
        UNUSED_PARAM(desc);
#ifndef NDEBUG
        description = desc;
#endif
    }

#if ENABLE(JSC_MULTIPLE_THREADS)

    void RefCountedLeakCounter::increment()
    {
#ifndef NDEBUG
        atomicIncrement(&count);
#endif
    }

    void RefCountedLeakCounter::decrement()
    {
#ifndef NDEBUG
        atomicDecrement(&count);
#endif
    }

#else

    void RefCountedLeakCounter::increment()
    {
#ifndef NDEBUG
        ++count;
#endif
    }

    void RefCountedLeakCounter::decrement()
    {
#ifndef NDEBUG
        --count;
#endif
    }

#endif

} // Namespace WTF
