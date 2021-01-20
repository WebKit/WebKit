/*
    Copyright (C) 2005 Apple Inc. All rights reserved.

    This library is free software; you can redistribute it and/or
    modify it under the terms of the GNU Library General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.

    This library is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
    Library General Public License for more details.

    You should have received a copy of the GNU Library General Public License
    along with this library; see the file COPYING.LIB.  If not, write to
    the Free Software Foundation, Inc., 51 Franklin Street, Fifth Floor,
    Boston, MA 02110-1301, USA.
*/

#include "config.h"
#include <wtf/HashTable.h>

namespace WTF {

DEFINE_ALLOCATOR_WITH_HEAP_IDENTIFIER(HashTable);

#if DUMP_HASHTABLE_STATS

std::atomic<unsigned> HashTableStats::numAccesses;
std::atomic<unsigned> HashTableStats::numRehashes;
std::atomic<unsigned> HashTableStats::numRemoves;
std::atomic<unsigned> HashTableStats::numReinserts;

unsigned HashTableStats::numCollisions;
unsigned HashTableStats::collisionGraph[4096];
unsigned HashTableStats::maxCollisions;

static Lock hashTableStatsMutex;

void HashTableStats::recordCollisionAtCount(unsigned count)
{
    auto locker = holdLock(hashTableStatsMutex);

    if (count > maxCollisions)
        maxCollisions = count;
    numCollisions++;
    collisionGraph[count]++;
}

void HashTableStats::dumpStats()
{
    auto locker = holdLock(hashTableStatsMutex);

    dataLogF("\nWTF::HashTable statistics\n\n");
    dataLogF("%u accesses\n", numAccesses.load());
    dataLogF("%d total collisions, average %.2f probes per access\n", numCollisions, 1.0 * (numAccesses + numCollisions) / numAccesses);
    dataLogF("longest collision chain: %d\n", maxCollisions);
    for (unsigned i = 1; i <= maxCollisions; i++) {
        dataLogF("  %u lookups with exactly %u collisions (%.2f%% , %.2f%% with this many or more)\n", collisionGraph[i], i, 100.0 * (collisionGraph[i] - collisionGraph[i+1]) / numAccesses, 100.0 * collisionGraph[i] / numAccesses);
    }
    dataLogF("%d rehashes\n", numRehashes.load());
    dataLogF("%d reinserts\n", numReinserts.load());
}

#endif

} // namespace WTF
