/*
 * Copyright (c) 2018-2019 Apple Inc. All rights reserved.
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

#include "LargeSharingPoolDump.h"

#if TLC

#include "pas_page_sharing_pool.h"

using namespace std;

namespace {

bool forEachAdapter(pas_large_sharing_node* node,
                    void* arg)
{
    function<bool(pas_large_sharing_node*)>* visitor =
        static_cast<function<bool(pas_large_sharing_node*)>*>(arg);
    
    return (*visitor)(node);
}

} // anonymous namespace

void forEachLargeSharingPoolNode(function<bool(pas_large_sharing_node*)> visitor)
{
    pas_large_sharing_pool_for_each(forEachAdapter, &visitor, pas_lock_is_not_held);
}

vector<pas_large_sharing_node*> largeSharingPoolAsVector()
{
    vector<pas_large_sharing_node*> result;
    forEachLargeSharingPoolNode([&] (pas_large_sharing_node* node) -> bool {
        result.push_back(node);
        return true;
    });
    return result;
}

void dumpLargeSharingPool()
{
    cout << "Large sharing pool:\n";
    cout.flush();
    forEachLargeSharingPoolNode(
        [&] (pas_large_sharing_node* node) -> bool {
            cout << "    " << reinterpret_cast<void*>(node->range.begin)
                 << "..." << reinterpret_cast<void*>(node->range.end) << ": use_epoch = "
                 << node->use_epoch << ", num_live_bytes = " << node->num_live_bytes
                 << ", is_committed = " << !!node->is_committed << "\n";
            cout.flush();
            return true;
        });
    cout << "Physical balance: " << pas_physical_page_sharing_pool_balance << "\n";
    cout.flush();
}

#endif // TLC
