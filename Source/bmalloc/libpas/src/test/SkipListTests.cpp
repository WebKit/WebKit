/*
 * Copyright (C) 2011-2020 Apple Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer. 
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution. 
 * 3.  Neither the name of Apple Inc. ("Apple") nor the names of
 *     its contributors may be used to endorse or promote products derived
 *     from this software without specific prior written permission. 
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE AND ITS CONTRIBUTORS "AS IS" AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
 * THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

// This is based on WebKit's TestWebkitAPI test for WTF's RedBlackTree.

#include "TestHarness.h"
#include <algorithm>
#include "pas_heap_lock.h"
#include "pas_immortal_heap.h"
#include "pas_skip_list_inlines.h"
#include <sstream>
#include <vector>

using namespace std;

namespace {

static constexpr bool verbose = false;

struct TestNode {
    TestNode(char key, unsigned value)
        : key(key)
        , value(value)
    {
    }
    
    char key;
    unsigned value;

    pas_skip_list_node node;

    static TestNode* fromSkipListNode(pas_skip_list_node* node)
    {
        if (!node)
            return NULL;
        return reinterpret_cast<TestNode*>(reinterpret_cast<char*>(node) -
                                           PAS_OFFSETOF(TestNode, node));
    }

    static int compareKey(pas_skip_list_node* aNode, void* bRaw)
    {
        TestNode* a = fromSkipListNode(aNode);
        char b = static_cast<char>(reinterpret_cast<uintptr_t>(bRaw));
        
        if (a->key < b)
            return -1;
        if (a->key == b)
            return 0;
        return 1;
    }

    static int compareKeyLeastGreaterThanOrEqual(pas_skip_list_node* aNode, void* bRaw)
    {
        TestNode* a = fromSkipListNode(aNode);
        char b = static_cast<char>(reinterpret_cast<uintptr_t>(bRaw));
        
        if (a->key < b)
            return -1;
        return 1;
    }

    static int compareKeyForInsert(pas_skip_list_node* aNode, void* bRaw)
    {
        TestNode* a = fromSkipListNode(aNode);
        char b = static_cast<char>(reinterpret_cast<uintptr_t>(bRaw));
        
        if (a->key <= b)
            return -1;
        return 1;
    }

    static int compare(pas_skip_list_node* aNode,
                       pas_skip_list_node* bNode)
    {
        TestNode* a = fromSkipListNode(aNode);
        TestNode* b = fromSkipListNode(bNode);

        if (a->key < b->key)
            return -1;
        if (a->key == b->key)
            return 0;
        return 1;
    }
};

static unsigned counter;

struct Pair {
    Pair() = default;
    
    Pair(char key, unsigned value)
        : key(key)
        , value(value)
    {
    }
    
    bool operator==(const Pair& other) const
    {
        return key == other.key;
    }
    
    bool operator<(const Pair& other) const
    {
        return key < other.key;
    }
    
    char key;
    unsigned value;
};

typedef vector<Pair> PairVector;

PairVector findExact(PairVector& asVector, char key)
{
    PairVector result;
    
    for (size_t index = 0; index < asVector.size(); ++index) {
        if (asVector[index].key == key)
            result.push_back(asVector[index]);
    }
    
    sort(result.begin(), result.end());
    
    return result;
}

void remove(PairVector& asVector, size_t index)
{
    asVector[index] = asVector.back();
    asVector.pop_back();
}

PairVector findLeastGreaterThanOrEqual(PairVector& asVector, char key)
{
    char bestKey = 0; // assignment to make gcc happy
    bool foundKey = false;
    
    for (size_t index = 0; index < asVector.size(); ++index) {
        if (asVector[index].key >= key) {
            if (asVector[index].key < bestKey || !foundKey) {
                foundKey = true;
                bestKey = asVector[index].key;
            }
        }
    }
    
    PairVector result;
    
    if (!foundKey)
        return result;
    
    return findExact(asVector, bestKey);
}

void assertFoundAndRemove(PairVector& asVector, char key, unsigned value)
{
    bool found = false;
    size_t foundIndex = 0; // make compilers happy
    
    for (size_t index = 0; index < asVector.size(); ++index) {
        if (asVector[index].key == key
            && asVector[index].value == value) {
            CHECK(!found);
            
            found = true;
            foundIndex = index;
        }
    }
    
    CHECK(found);
    
    remove(asVector, foundIndex);
}

// This deliberately passes a copy of the vector.
void assertEqual(pas_skip_list& asSkipList, PairVector asVector)
{
    for (TestNode* current = TestNode::fromSkipListNode(pas_skip_list_head(&asSkipList));
         current;
         current = TestNode::fromSkipListNode(pas_skip_list_node_next(&current->node)))
        assertFoundAndRemove(asVector, current->key, current->value);
    CHECK(asVector.empty());
}

void assertSameValuesForKey(pas_skip_list& asSkipList, TestNode* node,
                            PairVector foundValues, char key)
{
    if (node) {
        CHECK_EQUAL(node->key, key);
        
        TestNode* prevNode = node;
        do {
            node = prevNode;
            prevNode = TestNode::fromSkipListNode(pas_skip_list_node_prev(&prevNode->node));
        } while (prevNode && prevNode->key == key);
        
        if (verbose)
            printf("in assertSameValuesForKey, starting the search: node = %p\n", node);
        
        CHECK_EQUAL(node->key, key);
        CHECK(!prevNode || prevNode->key < key);
        
        do {
            if (verbose)
                printf("node = %p\n", node);
            
            assertFoundAndRemove(foundValues, node->key, node->value);
            
            node = TestNode::fromSkipListNode(pas_skip_list_node_next(&node->node));
            CHECK(!node || node->key >= key);
        } while (node && node->key == key);
    }
    
    CHECK(foundValues.empty());
}

void dumpSkipList(pas_skip_list& asSkipList, const char* prefix)
{
    unsigned maxHeight = 0;
    for (pas_skip_list_node* node = pas_skip_list_head(&asSkipList);
         node;
         node = pas_skip_list_node_next(node))
        maxHeight = PAS_MAX(maxHeight, node->height);
    for (unsigned index = maxHeight; index--;) {
        cout << prefix;
        for (pas_skip_list_node* node = pas_skip_list_head(&asSkipList);
             node;
             node = pas_skip_list_node_next(node)) {
            if (node->height <= index) {
                cout << " ";
                continue;
            }
            cout << TestNode::fromSkipListNode(node)->key;
        }
        cout << "\n";
    }
}

void dumpVector(PairVector vector)
{
    for (const Pair& pair : vector) {
        cout << pair.key;
    }
}

// The control string is a null-terminated list of commands. Each
// command is two characters, with the first identifying the operation
// and the second giving a key. The commands are:
//  +x  Add x
//  *x  Find all elements equal to x
//  @x  Find all elements that have the smallest key that is greater than or equal to x
//  !x  Remove all elements equal to x
void testDriver(const char* controlString)
{
    PairVector asVector;
    pas_skip_list asSkipList;
    
    pas_skip_list_construct(&asSkipList);
    
    for (const char* current = controlString; *current; current += 2) {
        char command = current[0];
        char key = current[1];
        unsigned value = ++counter;
            
        CHECK(command);
        CHECK(key);
        
        if (verbose)
            printf("Running command: %c%c\n", command, key);
            
        switch (command) {
        case '+': {
            pas_heap_lock_lock();
            TestNode* node = static_cast<TestNode*>(
                pas_skip_list_node_allocate(PAS_OFFSETOF(TestNode, node)));
            new (node) TestNode(key, value);
            pas_skip_list_insert(
                &asSkipList,
                reinterpret_cast<void*>(static_cast<uintptr_t>(key)),
                &node->node,
                TestNode::compareKeyForInsert);
            pas_heap_lock_unlock();
            asVector.push_back(Pair(key, value));
            break;
        }
                
        case '*': {
            pas_skip_list_find_result findResult = pas_skip_list_find(
                &asSkipList,
                reinterpret_cast<void*>(static_cast<uintptr_t>(key)),
                TestNode::compareKey);
            TestNode* node;
            if (findResult.did_find_exact)
                node = TestNode::fromSkipListNode(findResult.next);
            else
                node = nullptr;
            if (verbose)
                printf("Found node = %p\n", node);
            if (node)
                CHECK_EQUAL(node->key, key);
            assertSameValuesForKey(asSkipList, node, findExact(asVector, key), key);
            break;
        }

        case '@': {
            pas_skip_list_find_result findResult = pas_skip_list_find(
                &asSkipList,
                reinterpret_cast<void*>(static_cast<uintptr_t>(key)),
                TestNode::compareKeyLeastGreaterThanOrEqual);
            if (verbose) {
                cout << "SkipList:\n";
                dumpSkipList(asSkipList, "    ");
                cout << "Vector: ";
                dumpVector(asVector);
                cout << "\n";
            }
            CHECK(!findResult.did_find_exact);
            TestNode* node = TestNode::fromSkipListNode(findResult.next);
            if (verbose)
                printf("Found node = %p\n", node);
            if (node) {
                CHECK_GREATER_EQUAL(node->key, key);
                assertSameValuesForKey(asSkipList, node, findLeastGreaterThanOrEqual(asVector, key), node->key);
            } else
                CHECK(findLeastGreaterThanOrEqual(asVector, key).empty());
            break;
        }
                
        case '!': {
            while (true) {
                pas_skip_list_find_result findResult = pas_skip_list_find(
                    &asSkipList,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(key)),
                    TestNode::compareKey);
                TestNode* node;
                if (findResult.did_find_exact)
                    node = TestNode::fromSkipListNode(findResult.next);
                else
                    node = nullptr;
                if (node) {
                    pas_skip_list_remove(&asSkipList, &node->node);
                    CHECK_EQUAL(node->key, key);
                    assertFoundAndRemove(asVector, node->key, node->value);
                } else {
                    CHECK(findExact(asVector, key).empty());
                    break;
                }
            }
            break;
        }
                
        default:
            CHECK(!"bad command");
            break;
        }
            
        CHECK_EQUAL(pas_skip_list_size(&asSkipList), asVector.size());
        pas_skip_list_validate(&asSkipList, TestNode::compare);
        assertEqual(asSkipList, asVector);
    }
}

void randomTestDriver(unsigned numAddOperations, unsigned numOperations)
{
    ostringstream out;
    const char* things = "abcdefghijklmnopqrstuvwxyzQWERTYUIOPASDFGHJKLZXCVBNM1234567890";
    const char* operations = "+*@!";
    auto randomChar =
        [] (const char* things) -> char {
            return things[deterministicRandomNumber(static_cast<unsigned>(strlen(things)))];
        };
    for (unsigned index = numAddOperations; index--;) {
        out << "+" << randomChar(things);
    }
    for (unsigned index = numOperations; index--;) {
        out << randomChar(operations) << randomChar(things);
    }
    string str = out.str();
    if (verbose)
        cout << "    Operation string: " << str << "\n";
    testDriver(str.c_str());
}

} // anonymous namespace

void addSkipListTests()
{
    ADD_TEST(testDriver(""));
    ADD_TEST(testDriver("*x@y!z"));
    ADD_TEST(testDriver("+a"));
    ADD_TEST(testDriver("+a*x@y!z"));
    ADD_TEST(testDriver("+a*a@a!a"));
    ADD_TEST(testDriver("+a+b"));
    ADD_TEST(testDriver("+a+b+c"));
    ADD_TEST(testDriver("+a+b+c+d"));
    ADD_TEST(testDriver("+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d"));
    ADD_TEST(testDriver("+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t+u+v+x+y+z"));
    ADD_TEST(testDriver("+z+y+x+w+v+u+t+s+r+q+p+o+n+m+l+k+j+i+h+g+f+e+d+c+b+a*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!a!b!c!d!e!f!g!h!i!j!k!l!m!n!o!p!q!r!s!t!u!v!w!x!y!z"));
    ADD_TEST(testDriver("+z+y+x+w+v+u+t+s+r+q+p+o+n+m+l+k+j+i+h+g+f+e+d+c+b+a*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!z!y!x!w!v!u!t!s!r!q!p!o!n!m!l!k!j!i!h!g!f!e!d!c!b!a"));
    ADD_TEST(testDriver("+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+a+b+c+d+e+f+g+h+i+j+k+l+m+n+o+p+q+r+s+t+u+v+x+y+z*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!a!b!c!d!e!f!g!h!i!j!k!l!m!n!o!p!q!r!s!t!u!v!w!x!y!z"));
    ADD_TEST(testDriver("+d+d+m+w@d@m@w@a@g@q"));
    ADD_TEST(testDriver("+d+d+d+d+d+d+d+d+d+d+f+f+f+f+f+f+f+h+h+i+j+k+l+m+o+p+q+r+z@a@b@c@d@e@f@g@h@i@j@k@l@m@n@o@p@q@r@s@t@u@v@w@x@y@z"));
    ADD_TEST(randomTestDriver(10, 1000));
    ADD_TEST(randomTestDriver(20, 1000));
    ADD_TEST(randomTestDriver(30, 1000));
    ADD_TEST(randomTestDriver(40, 1000));
    ADD_TEST(randomTestDriver(50, 1000));
    ADD_TEST(randomTestDriver(100, 1000));
    ADD_TEST(randomTestDriver(1000, 1000));
    ADD_TEST(randomTestDriver(10, 10000));
    ADD_TEST(randomTestDriver(20, 10000));
    ADD_TEST(randomTestDriver(100, 10000));
    ADD_TEST(randomTestDriver(1000, 10000));
}

