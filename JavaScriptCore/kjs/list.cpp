/*
 *  This file is part of the KDE libraries
 *  Copyright (C) 2002 Apple Computer, Inc.
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
 *  the Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA 02111-1307, USA.
 *
 */

#include "list.h"

#include "internal.h"

#define DUMP_STATISTICS 0

namespace KJS {

// tunable parameters
const int poolSize = 16; // must be a power of 2
const int inlineValuesSize = 4;

// derived constants
const int poolSizeMask = poolSize - 1;

enum ListImpState { unusedInPool = 0, usedInPool, usedOnHeap, immortal };

struct ListImp : ListImpBase
{
    ListImpState state;
    ValueImp *values[inlineValuesSize];
    int capacity;
    ValueImp **overflow;

#if DUMP_STATISTICS
    int sizeHighWaterMark;
#endif
};

static ListImp pool[poolSize];
static int poolCursor;

#if DUMP_STATISTICS

static int numLists;
static int numListsHighWaterMark;

static int listSizeHighWaterMark;

static int numListsDestroyed;
static int numListsBiggerThan[17];

struct ListStatisticsExitLogger { ~ListStatisticsExitLogger(); };

static ListStatisticsExitLogger logger;

ListStatisticsExitLogger::~ListStatisticsExitLogger()
{
    printf("\nKJS::List statistics:\n\n");
    printf("%d lists were allocated\n", numLists);
    printf("%d lists was the high water mark\n", numListsHighWaterMark);
    printf("largest list had %d elements\n", listSizeHighWaterMark);
    if (numListsDestroyed) {
        putc('\n', stdout);
        for (int i = 0; i < 17; i++) {
            printf("%.1f%% of the lists (%d) had more than %d element%s\n",
                100.0 * numListsBiggerThan[i] / numListsDestroyed,
                numListsBiggerThan[i],
                i, i == 1 ? "" : "s");
        }
        putc('\n', stdout);
    }
}

#endif

static inline ListImp *allocateListImp()
{
    // Find a free one in the pool.
    int c = poolCursor;
    int i = c;
    do {
        ListImp *imp = &pool[i];
        ListImpState s = imp->state;
        i = (i + 1) & poolSizeMask;
        if (s == unusedInPool) {
            poolCursor = i;
            imp->state = usedInPool;
            return imp;
        }
    } while (i != c);
    
    ListImp *imp = new ListImp;
    imp->state = usedOnHeap;
    return imp;
}

static inline void deallocateListImp(ListImp *imp)
{
    if (imp->state == usedInPool)
        imp->state = unusedInPool;
    else
        delete imp;
}

List::List() : _impBase(allocateListImp())
{
    ListImp *imp = static_cast<ListImp *>(_impBase);
    imp->size = 0;
    imp->refCount = 1;
    imp->capacity = 0;
    imp->overflow = 0;

#if DUMP_STATISTICS
    if (++numLists > numListsHighWaterMark)
        numListsHighWaterMark = numLists;
    imp->sizeHighWaterMark = 0;
#endif
}

inline void List::derefValues()
{
    ListImp *imp = static_cast<ListImp *>(_impBase);
    
    int size = imp->size;
    
    int inlineSize = MIN(size, inlineValuesSize);
    for (int i = 0; i != inlineSize; ++i)
        imp->values[i]->deref();
    
    int overflowSize = size - inlineSize;
    ValueImp **overflow = imp->overflow;
    for (int i = 0; i != overflowSize; ++i)
        overflow[i]->deref();
}

void List::release()
{
    ListImp *imp = static_cast<ListImp *>(_impBase);
    
#if DUMP_STATISTICS
    --numLists;
    ++numListsDestroyed;
    for (int i = 0; i < 17; i++)
        if (imp->sizeHighWaterMark > i)
            ++numListsBiggerThan[i];
#endif

    derefValues();
    delete [] imp->overflow;
    deallocateListImp(imp);
}

ValueImp *List::impAt(int i) const
{
    ListImp *imp = static_cast<ListImp *>(_impBase);
    if ((unsigned)i >= (unsigned)imp->size)
        return UndefinedImp::staticUndefined;
    if (i < inlineValuesSize)
        return imp->values[i];
    return imp->overflow[i - inlineValuesSize];
}

void List::clear()
{
    derefValues();
    _impBase->size = 0;
}

void List::append(ValueImp *v)
{
    ListImp *imp = static_cast<ListImp *>(_impBase);

    int i = imp->size++;

#if DUMP_STATISTICS
    if (imp->size > listSizeHighWaterMark)
        listSizeHighWaterMark = imp->size;
#endif

    v->ref();
    
    if (i < inlineValuesSize) {
        imp->values[i] = v;
        return;
    }
    
    if (i >= imp->capacity) {
        int newCapacity = i * 2;
        ValueImp **newOverflow = new ValueImp * [newCapacity - inlineValuesSize];
        ValueImp **oldOverflow = imp->overflow;
        int oldOverflowSize = i - inlineValuesSize;
        for (int j = 0; j != oldOverflowSize; j++)
            newOverflow[j] = oldOverflow[j];
        delete [] oldOverflow;
        imp->overflow = newOverflow;
        imp->capacity = newCapacity;
    }
    
    imp->overflow[i - inlineValuesSize] = v;
}

List List::copyTail() const
{
    List copy;

    ListImp *imp = static_cast<ListImp *>(_impBase);

    int size = imp->size;

    int inlineSize = MIN(size, inlineValuesSize);
    for (int i = 0; i != inlineSize; ++i)
        copy.append(imp->values[i]);

    ValueImp **overflow = imp->overflow;
    int overflowSize = size - inlineSize;
    for (int i = 0; i != overflowSize; ++i)
        copy.append(overflow[i]);

    return copy;
}

const List &List::empty()
{
    static List emptyList;
    return emptyList;
}

} // namespace KJS
