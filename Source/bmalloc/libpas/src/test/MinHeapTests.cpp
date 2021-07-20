/*
 * Copyright (c) 2019-2020 Apple Inc. All rights reserved.
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

#include "TestHarness.h"
#include "pas_bootstrap_free_heap.h"
#include "pas_min_heap.h"
#include <set>
#include <vector>

using namespace std;

namespace {

constexpr bool verbose = false;

struct Element {
    Element() = default;
    
    Element(unsigned char c)
        : c(c)
    {
    }
    
    unsigned char c { 0 };
    size_t index { 0 };
};

unsigned char asChar(void* value)
{
    return static_cast<Element*>(value)->c;
}

int compare(Element** left, Element** right)
{
    if (asChar(*left) < asChar(*right))
        return -1;
    if (asChar(*left) == asChar(*right))
        return 0;
    return 1;
}

size_t getIndex(Element** element)
{
    return (*element)->index;
}

void setIndex(Element** element, size_t index)
{
    (*element)->index = index;
}

PAS_CREATE_MIN_HEAP(
    MinHeap,
    Element*,
    5,
    .compare = compare,
    .get_index = getIndex,
    .set_index = setIndex);

void dumpHeap(MinHeap& minHeap)
{
    cout << "    min heap:\n";
    if (!minHeap.size) {
        cout << "        <empty>\n";
        return;
    }
    // Guide:
    //        x
    //    x       x
    //  x   x   x   x
    // x x x x x x x x
    size_t numLevels = pas_log2(minHeap.size) + 1;
    size_t numColumns = 1 << (numLevels - 1);
    size_t index = 1;
    for (size_t levelIndex = 0; levelIndex < numLevels; ++levelIndex) {
        cout << "        ";
        size_t numElements = 1 << levelIndex;
        for (size_t elementIndex = 0; elementIndex < numElements; elementIndex++) {
            size_t numSpaces = numColumns/numElements * 2 - 1;
            if (!elementIndex)
                numSpaces /= 2;
            for (size_t spaceIndex = numSpaces; spaceIndex--;)
                cout << " ";
            if (index <= minHeap.size)
                cout << (*MinHeap_get_ptr_by_index(&minHeap, index++))->c;
            else
                PAS_ASSERT(levelIndex + 1 == numLevels);
        }
        cout << "\n";
    }
    PAS_ASSERT(index == minHeap.size + 1);
}

void testMinHeap(const char* program)
{
    MinHeap minHeap;
    MinHeap_construct(&minHeap);
    
    for (size_t i = 0; program[i]; ++i) {
        char command = program[i];
        
        char value = 0;
        
        auto getValue = [&] () {
            value = program[++i];
            PAS_ASSERT(value);
        };
        
        switch (command) {
        case '+': {
            getValue();
            Element* thing = new Element(value);
            MinHeap_add(&minHeap, thing, &allocationConfig);
            CHECK(thing->index);
            if (verbose) {
                cout << "  After +" << value << ":\n";
                dumpHeap(minHeap);
            }
            break;
        }
            
        case '!': {
            CHECK(!MinHeap_take_min(&minHeap));
            break;
        }
            
        case '-': {
            getValue();
            if (verbose) {
                cout << "  Before -" << value << ":\n";
                dumpHeap(minHeap);
            }
            Element* thing = MinHeap_take_min(&minHeap);
            CHECK_EQUAL(thing->c, value);
            CHECK(!thing->index);
            break;
        }
            
        case '*': {
            getValue();
            if (verbose) {
                cout << "  Before *" << value << ":\n";
                dumpHeap(minHeap);
            }
            size_t foundIndex = 0;
            bool found = false;
            for (size_t i = minHeap.size; i; i--) {
                Element* element = *MinHeap_get_ptr_by_index(&minHeap, i);
                if (element->c == value) {
                    CHECK(!found);
                    foundIndex = i;
                    found = true;
                }
            }
            Element* element = MinHeap_remove_by_index(&minHeap, foundIndex);
            CHECK_EQUAL(element->c, value);
            break;
        }
            
        default:
            PAS_ASSERT(!"Should not be reached");
        }
    }
    
    MinHeap_destruct(&minHeap, &allocationConfig);
}

void minHeapChaos(unsigned initialSize, unsigned numOperations,
                  bool randomRemove,
                  const pas_allocation_config& myAllocationConfig)
{
    MinHeap minHeap;
    MinHeap_construct(&minHeap);
    
    multiset<unsigned char> standard;
    
    for (unsigned i = initialSize; i--;) {
        char c = static_cast<char>(deterministicRandomNumber(256));
        Element* thing = new Element(c);
        MinHeap_add(&minHeap, thing, &myAllocationConfig);
        standard.insert(c);
    }
    
    CHECK_EQUAL(static_cast<unsigned>(MinHeap_get_min(&minHeap)->c),
                static_cast<unsigned>(*standard.begin()));
    
    for (unsigned i = numOperations; i--;) {
        CHECK_EQUAL(minHeap.size, standard.size());
        
        switch (deterministicRandomNumber(2)) {
        case 0: {
            char c = static_cast<char>(deterministicRandomNumber(256));
            Element* thing = new Element(c);
            MinHeap_add(&minHeap, thing, &myAllocationConfig);
            standard.insert(c);
            break;
        }
        case 1: {
            if (standard.empty()) {
                PAS_ASSERT(!minHeap.size);
                CHECK(!MinHeap_take_min(&minHeap));
                break;
            }
            switch (randomRemove ? deterministicRandomNumber(2) : 0) {
            case 0: {
                Element* thing = MinHeap_take_min(&minHeap);
                CHECK(thing);
                auto iter = standard.begin();
                CHECK_EQUAL(static_cast<unsigned>(thing->c), static_cast<unsigned>(*iter));
                standard.erase(iter);
                break;
            }
            case 1: {
                Element* thing = *MinHeap_get_ptr_by_index(
                    &minHeap,
                    1 + deterministicRandomNumber(static_cast<unsigned>(minHeap.size)));
                standard.erase(standard.find(thing->c));
                MinHeap_remove(&minHeap, thing);
                break;
            }
            default:
                PAS_ASSERT(!"Should not be reached");
                break;
            }
            break;
        }
        default:
            PAS_ASSERT(!"Should not be reached");
            break;
        }
    }
    
    for (;;) {
        CHECK_EQUAL(minHeap.size, standard.size());
        
        if (!minHeap.size)
            break;
        
        Element* thing = MinHeap_take_min(&minHeap);
        CHECK(thing);
        auto iter = standard.begin();
        CHECK_EQUAL(static_cast<unsigned>(thing->c), static_cast<unsigned>(*iter));
        standard.erase(iter);
    }
}

void testMinHeapChaos(unsigned initialSize, unsigned numOperations, bool randomRemove)
{
    minHeapChaos(initialSize, numOperations, randomRemove, allocationConfig);
}

pas_allocation_config makeBootstrapAllocationConfig()
{
    pas_allocation_config result;
    pas_bootstrap_free_heap_allocation_config_construct(&result, pas_lock_is_not_held);
    return result;
}

void testBootstrapMinHeapChaos(unsigned initialSize, unsigned numOperations, bool randomRemove)
{
    minHeapChaos(initialSize, numOperations, randomRemove, makeBootstrapAllocationConfig());
}

} // anonymous namespace

void addMinHeapTests()
{
    ADD_TEST(testMinHeap("!"));
    ADD_TEST(testMinHeap("+a"));
    ADD_TEST(testMinHeap("+a-a"));
    ADD_TEST(testMinHeap("!+a-a!"));
    ADD_TEST(testMinHeap("+a+b+c+d+e+f+g+h+i+j+k+l+m+n+i+o+p-a-b-c-d-e-f-g-h-i-i-j-k-l-m-n-o-p!"));
    ADD_TEST(testMinHeap("+j+b+d+l+o+f+p+n+i+a+k+c+m+h+i+e+g-a-b-c-d-e-f-g-h-i-i-j-k-l-m-n-o-p!"));
    ADD_TEST(testMinHeap("+a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t+u+v+w+x+y+z*z*y*x*w*v*u*t*s*r*q*p*o*n*m*l*k*j*i*h*g*f*e*d*c*b*a"));
    ADD_TEST(testMinHeap("+a+w+c+d+e+f+p+h+i+j+k+l+m+n+o+g+q+r+s+t+u+v+b+x+y+z*z*y*x*w*v*u*t*s*r*q*p*o*n*m*l*k*j*i*h*g*f*e*d*c*b*a"));
    ADD_TEST(testMinHeap("+a+w+c+d+e+f+p+h+i+j+k+l+m+n+o+g+q+r+s+t+u+v+b+x+y+z*a*y*c*w*e*u*g*s*r*q*p*o*n*m*l*k*j*i*h*t*f*v*d*x*b*z"));
    ADD_TEST(testMinHeap("+a+w+c+d+e+f+p+h+i+j+k+l+m+n+o+g+q+r+s+t+u+v+b+x+y+z*a*y*c*w*e*u*g*s*r*q*p*o*n*m*l-b-d-f-h-i-j-k-t-v-x-z"));
    ADD_TEST(testMinHeap("+z+y+x+w+v+u+t+s+r+q+p+o+n+m+l+k+j+i+h+g+f+e+d+c+b+a*a*y*c*w*e*u*g*s*r*q*p*o*n*m*l-b-d-f-h-i-j-k-t-v-x-z"));
    ADD_TEST(testMinHeap("+z+y+x+w+v+u+t+s+r+q+p+o+n+m+l+k+j+i+h+g+f+e+d+c+b+a*u*p*y*k-a-b-c-d-e-f-g-h-i-j-l-m-n-o-q-r-s-t-v-w-x-z"));
    ADD_TEST(testMinHeap("+z+y+x+w+v+u+t+s+r+q+p+o+n+m+l+k+j+i+h+g+f+e+d+c+b+a*u*p*y*q-a-b-c-d-e-f-g-h-i-j-k-l-m-n-o-r-s-t-v-w-x-z"));
    ADD_TEST(testMinHeap("+a+b+c+d*d-a-b-c!"));
    ADD_TEST(testMinHeapChaos(100, 1000, false));
    ADD_TEST(testMinHeapChaos(1000, 10000, false));
    ADD_TEST(testMinHeapChaos(10000, 10000, false));
    ADD_TEST(testBootstrapMinHeapChaos(100, 1000, false));
    ADD_TEST(testBootstrapMinHeapChaos(1000, 10000, false));
    ADD_TEST(testBootstrapMinHeapChaos(10000, 10000, false));
    ADD_TEST(testMinHeapChaos(100, 1000, true));
    ADD_TEST(testMinHeapChaos(1000, 10000, true));
    ADD_TEST(testMinHeapChaos(10000, 10000, true));
    ADD_TEST(testBootstrapMinHeapChaos(100, 1000, true));
    ADD_TEST(testBootstrapMinHeapChaos(1000, 10000, true));
    ADD_TEST(testBootstrapMinHeapChaos(10000, 10000, true));
    ADD_TEST(testBootstrapMinHeapChaos(10000, 100000, true));
    ADD_TEST(testBootstrapMinHeapChaos(100000, 100000, true));
}

