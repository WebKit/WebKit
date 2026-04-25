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
#include "pas_cartesian_tree.h"
#include "pas_heap_lock.h"
#include "pas_utility_heap.h"
#include <sstream>
#include <vector>

using namespace std;

namespace {

static constexpr bool verbose = false;

struct TestNode : public pas_cartesian_tree_node {
    TestNode(char xKey, char yKey, unsigned value)
        : xKey(xKey)
        , yKey(yKey)
        , value(value)
    {
    }
    
    char xKey;
    char yKey;
    unsigned value;

    static int compare(void* aRaw, void* bRaw)
    {
        char a = static_cast<char>(reinterpret_cast<uintptr_t>(aRaw));
        char b = static_cast<char>(reinterpret_cast<uintptr_t>(bRaw));
        
        if (a < b)
            return -1;
        if (a == b)
            return 0;
        return 1;
    }

    static void* getXKey(pas_cartesian_tree_node* node)
    {
        return reinterpret_cast<void*>(
            static_cast<uintptr_t>(reinterpret_cast<TestNode*>(node)->xKey));
    }

    static void* getYKey(pas_cartesian_tree_node* node)
    {
        return reinterpret_cast<void*>(
            static_cast<uintptr_t>(reinterpret_cast<TestNode*>(node)->yKey));
    }
};

static pas_cartesian_tree_config config = {
    .get_x_key = TestNode::getXKey,
    .get_y_key = TestNode::getYKey,
    .x_compare = TestNode::compare,
    .y_compare = TestNode::compare
};

static unsigned counter;

struct Pair {
    Pair() = default;
    
    Pair(char xKey, char yKey, unsigned value)
        : xKey(xKey)
        , yKey(yKey)
        , value(value)
    {
    }
    
    bool operator==(const Pair& other) const
    {
        return xKey == other.xKey;
    }
    
    bool operator<(const Pair& other) const
    {
        return xKey < other.xKey;
    }
    
    char xKey;
    char yKey;
    unsigned value;
};

typedef vector<Pair> PairVector;

PairVector findExact(PairVector& asVector, char xKey)
{
    PairVector result;
    
    for (size_t index = 0; index < asVector.size(); ++index) {
        if (asVector[index].xKey == xKey)
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

PairVector findLeastGreaterThanOrEqual(PairVector& asVector, char xKey)
{
    char bestKey = 0; // assignment to make gcc happy
    bool foundKey = false;
    
    for (size_t index = 0; index < asVector.size(); ++index) {
        if (asVector[index].xKey >= xKey) {
            if (asVector[index].xKey < bestKey || !foundKey) {
                foundKey = true;
                bestKey = asVector[index].xKey;
            }
        }
    }
    
    PairVector result;
    
    if (!foundKey)
        return result;
    
    return findExact(asVector, bestKey);
}

void assertFoundAndRemove(PairVector& asVector, char xKey, char yKey, unsigned value)
{
    bool found = false;
    size_t foundIndex = 0; // make compilers happy
    
    for (size_t index = 0; index < asVector.size(); ++index) {
        if (asVector[index].xKey == xKey
            && asVector[index].yKey == yKey
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
void assertEqual(pas_cartesian_tree& asCartesianTree, PairVector asVector)
{
    for (TestNode* current = reinterpret_cast<TestNode*>(
             pas_cartesian_tree_minimum(&asCartesianTree));
         current;
         current = reinterpret_cast<TestNode*>(pas_cartesian_tree_node_successor(current)))
        assertFoundAndRemove(asVector, current->xKey, current->yKey, current->value);
    CHECK(asVector.empty());
}

void assertSameValuesForKey(pas_cartesian_tree& asCartesianTree, TestNode* node,
                            PairVector foundValues, char xKey)
{
    if (node) {
        CHECK_EQUAL(node->xKey, xKey);
        
        TestNode* prevNode = node;
        do {
            node = prevNode;
            prevNode = reinterpret_cast<TestNode*>(pas_cartesian_tree_node_predecessor(prevNode));
        } while (prevNode && prevNode->xKey == xKey);
        
        if (verbose)
            printf("in assertSameValuesForKey, starting the search: node = %p\n", node);
        
        CHECK_EQUAL(node->xKey, xKey);
        CHECK(!prevNode || prevNode->xKey < xKey);
        
        do {
            if (verbose)
                printf("node = %p\n", node);
            
            assertFoundAndRemove(foundValues, node->xKey, node->yKey, node->value);
            
            node = reinterpret_cast<TestNode*>(pas_cartesian_tree_node_successor(node));
            CHECK(!node || node->xKey >= xKey);
        } while (node && node->xKey == xKey);
    }
    
    CHECK(foundValues.empty());
}

static constexpr unsigned nodeDumpWidth = 2;

unsigned cartesianTreePrintWidth(pas_cartesian_tree_node* node)
{
    if (!node)
        return 0;
    return nodeDumpWidth
        + cartesianTreePrintWidth(pas_compact_cartesian_tree_node_ptr_load(&node->left))
        + cartesianTreePrintWidth(pas_compact_cartesian_tree_node_ptr_load(&node->right));
}

void printSpaces(unsigned count)
{
    while (count--)
        cout << " ";
}

void dumpCartesianTree(pas_cartesian_tree& asCartesianTree, const char* prefix)
{
    struct WorkItem {
        WorkItem() = default;
        
        explicit WorkItem(pas_cartesian_tree_node* node)
            : node(node)
            , spaces(0)
        {
        }

        explicit WorkItem(unsigned spaces)
            : node(nullptr)
            , spaces(spaces)
        {
        }

        pas_cartesian_tree_node* node;
        unsigned spaces;
    };

    vector<WorkItem> workItems;
    bool hasRealWork = false;

    if (pas_cartesian_tree_node* node = pas_compact_cartesian_tree_node_ptr_load(
            &asCartesianTree.root)) {
        workItems.push_back(WorkItem(node));
        hasRealWork = true;
    }

    while (hasRealWork) {
        vector<WorkItem> currentWorkItems;
        swap(workItems, currentWorkItems);
        hasRealWork = false;

        cout << prefix;

        for (WorkItem workItem : currentWorkItems) {
            if (workItem.spaces) {
                PAS_ASSERT(!workItem.node);
                printSpaces(workItem.spaces);
                workItems.push_back(workItem);
                continue;
            }
            auto handleChild =
                [&] (pas_cartesian_tree_node* child) {
                    if (!child)
                        return;
                    printSpaces(cartesianTreePrintWidth(child));
                    workItems.push_back(WorkItem(child));
                    hasRealWork = true;
                };
            handleChild(pas_compact_cartesian_tree_node_ptr_load(&workItem.node->left));
            TestNode* node = static_cast<TestNode*>(workItem.node);
            cout << node->xKey;
            cout << node->yKey;
            workItems.push_back(WorkItem(nodeDumpWidth));
            handleChild(pas_compact_cartesian_tree_node_ptr_load(&workItem.node->right));
        }

        cout << "\n";
    }
}

void dumpVector(PairVector vector)
{
    bool first = true;
    for (const Pair& pair : vector) {
        if (first)
            first = false;
        else
            cout << " ";
        cout << pair.xKey << pair.yKey;
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
    pas_cartesian_tree asCartesianTree;
    
    pas_cartesian_tree_construct(&asCartesianTree);
    
    for (const char* current = controlString; *current;) {
        char command = *current++;
        char xKey = *current++;
        char yKey = 0;
        if (command == '+')
            yKey = *current++;
        unsigned value = ++counter;
            
        PAS_ASSERT(command);
        PAS_ASSERT(xKey);
        if (command == '+')
            PAS_ASSERT(yKey);
        
        if (verbose) {
            printf("Running command: %c%c", command, xKey);
            if (command == '+')
                printf("%c", yKey);
            printf(" with current = %p (%zu)\n", current, current - controlString);
        }
            
        switch (command) {
        case '+': {
            pas_heap_lock_lock();
            TestNode* node = static_cast<TestNode*>(
                pas_utility_heap_allocate(sizeof(TestNode), "TestNode"));
            if (verbose)
                cout << "Adding node = " << node << "\n";
            pas_heap_lock_unlock();
            new (node) TestNode(xKey, yKey, value);
            pas_cartesian_tree_insert(
                &asCartesianTree,
                reinterpret_cast<void*>(static_cast<uintptr_t>(xKey)),
                reinterpret_cast<void*>(static_cast<uintptr_t>(yKey)),
                node,
                &config);
            asVector.push_back(Pair(xKey, yKey, value));
            break;
        }
                
        case '*': {
            TestNode* node = reinterpret_cast<TestNode*>(
                pas_cartesian_tree_find_exact(
                    &asCartesianTree,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(xKey)),
                    &config));
            if (verbose)
                printf("Found node = %p\n", node);
            if (node)
                CHECK_EQUAL(node->xKey, xKey);
            assertSameValuesForKey(asCartesianTree, node, findExact(asVector, xKey), xKey);
            break;
        }

        case '@': {
            TestNode* node = reinterpret_cast<TestNode*>(
                pas_cartesian_tree_find_least_greater_than_or_equal(
                    &asCartesianTree,
                    reinterpret_cast<void*>(static_cast<uintptr_t>(xKey)),
                    &config));
            if (verbose) {
                cout << "CartesianTree:\n";
                dumpCartesianTree(asCartesianTree, "    ");
                cout << "Vector: ";
                dumpVector(asVector);
                cout << "\n";
            }
            if (verbose)
                printf("Found node = %p\n", node);
            if (node) {
                CHECK_GREATER_EQUAL(node->xKey, xKey);
                assertSameValuesForKey(asCartesianTree, node, findLeastGreaterThanOrEqual(asVector, xKey), node->xKey);
            } else
                CHECK(findLeastGreaterThanOrEqual(asVector, xKey).empty());
            break;
        }
                
        case '!': {
            while (true) {
                TestNode* node = static_cast<TestNode*>(
                    pas_cartesian_tree_find_exact(
                        &asCartesianTree,
                        reinterpret_cast<void*>(static_cast<uintptr_t>(xKey)),
                        &config));
                if (verbose) {
                    cout << "Found node = " << node;
                    if (node)
                        cout <<" (" << node->xKey << node->yKey << ")";
                    cout << " and removing it.\n";
                }
                if (node) {
                    pas_cartesian_tree_remove(&asCartesianTree, node, &config);
                    CHECK_EQUAL(node->xKey, xKey);
                    assertFoundAndRemove(asVector, node->xKey, node->yKey, node->value);
                    
                    if (verbose) {
                        cout << "Validating cartesian tree right after removal:\n";
                        dumpCartesianTree(asCartesianTree, "    ");
                    }
                    pas_cartesian_tree_validate(&asCartesianTree, &config);
                } else {
                    CHECK(findExact(asVector, xKey).empty());
                    break;
                }
            }
            break;
        }
                
        default:
            CHECK(!"bad command");
            break;
        }

        if (verbose) {
            cout << "Validating cartesian tree:\n";
            dumpCartesianTree(asCartesianTree, "    ");
        }
        pas_cartesian_tree_validate(&asCartesianTree, &config);
        CHECK_EQUAL(pas_cartesian_tree_size(&asCartesianTree), asVector.size());
        assertEqual(asCartesianTree, asVector);
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
        out << "+" << randomChar(things) << randomChar(things);
    }
    for (unsigned index = numOperations; index--;) {
        char operation = randomChar(operations);
        out << operation << randomChar(things);
        if (operation == '+')
            out << randomChar(things);
    }
    string str = out.str();
    if (verbose)
        cout << "    Operation string: " << str << "\n";
    testDriver(str.c_str());
}

} // anonymous namespace

void addCartesianTreeTests()
{
    ADD_TEST(testDriver(""));
    ADD_TEST(testDriver("*x@y!z"));
    ADD_TEST(testDriver("+aa"));
    ADD_TEST(testDriver("+aa*x@y!z"));
    ADD_TEST(testDriver("+aa*a@a!a"));
    ADD_TEST(testDriver("+aa+ba"));
    ADD_TEST(testDriver("+ab+ba"));
    ADD_TEST(testDriver("+aa+bb"));
    ADD_TEST(testDriver("+ab+bb"));
    ADD_TEST(testDriver("+aa+bb+cc"));
    ADD_TEST(testDriver("+ab+bc+ca"));
    ADD_TEST(testDriver("+ab+ba+cc"));
    ADD_TEST(testDriver("+ac+bb+ca"));
    ADD_TEST(testDriver("+az+by+cx+dw"));
    ADD_TEST(testDriver("+az+by+cx+dw!a"));
    ADD_TEST(testDriver("+aw+by+cx+dz"));
    ADD_TEST(testDriver("+aw+by+cx+dz!d"));
    ADD_TEST(testDriver("+az+by+dx+cw"));
    ADD_TEST(testDriver("+az+by+dx+cw!b"));
    ADD_TEST(testDriver("+az+bx+dy+cw"));
    ADD_TEST(testDriver("+az+bx+dy+cw!c"));
    ADD_TEST(testDriver("+az+dy+cx+bw"));
    ADD_TEST(testDriver("+az+dy+cx+bw!b"));
    ADD_TEST(testDriver("+ay+dz+cx+bw"));
    ADD_TEST(testDriver("+ay+dz+cx+bw!d"));
    ADD_TEST(testDriver("+az+dy+bx+cw"));
    ADD_TEST(testDriver("+az+dy+bx+cw!a"));
    ADD_TEST(testDriver("+ax+dy+bz+cw"));
    ADD_TEST(testDriver("+ax+dy+bz+cw!c"));
    ADD_TEST(testDriver("+dz+by+cx+aw"));
    ADD_TEST(testDriver("+dz+by+cx+aw!a"));
    ADD_TEST(testDriver("+dz+bw+cx+ay"));
    ADD_TEST(testDriver("+dz+bw+cx+ay!b"));
    ADD_TEST(testDriver("+dz+by+ax+cw"));
    ADD_TEST(testDriver("+dz+by+ax+cw!c"));
    ADD_TEST(testDriver("+dz+by+aw+cx"));
    ADD_TEST(testDriver("+dz+by+aw+cx!d"));
    ADD_TEST(testDriver("+dz+ay+cx+bw"));
    ADD_TEST(testDriver("+dz+ay+cx+bw!b"));
    ADD_TEST(testDriver("+dw+ax+cy+bz"));
    ADD_TEST(testDriver("+dw+ax+cy+bz!c"));
    ADD_TEST(testDriver("+dz+ay+bx+cw"));
    ADD_TEST(testDriver("+dz+ay+bx+cw!a"));
    ADD_TEST(testDriver("+dz+aw+by+cx"));
    ADD_TEST(testDriver("+dz+aw+by+cx!d"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!a!b!c!d"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!a!b!d!c"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!a!c!d!b"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!a!c!b!d"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!a!d!b!c"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!a!d!c!b"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!b!d!c!a"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!b!d!a!c"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!d!b!a!c"));
    ADD_TEST(testDriver("+ax+by+cz+dx+ay+bz+cx+dy+az+bx+cy+dz+ax+by+cz+dx+ay+bz+cm+dn+ao+bp+cq+dr+am+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq+ar+bn+co+dp+aq+br+cs+dm+an+bo+cp+dq!c!b!a!d"));
    ADD_TEST(testDriver("+az+by+cx+dw+av+bu+ct+ds+ar+bq+cp+do+an+bm+cl+dk+aj+bi+ch+dg+af+be+cd+dc+ab+ba+ca+db+ac+bd+ce+df+ag+bh+ci+dj+ak+bl+cm+dn+ax+by+cz+dz+ay+bx+cf+dg+eh+fi+gf+hg+ih+ji+ka+lb+mc+nd+oe+pf+qg+rh+si+tj+uk+vl+xm+yn+zo"));
    ADD_TEST(testDriver("+az+by+cx+dw+av+bu+ct+ds+ar+bq+cp+do+an+bm+cl+dk+aj+bi+ch+dg+af+be+cd+dc+ab+ba+ca+db+ac+bd+ce+df+ag+bh+ci+dj+ak+bl+cm+dn+ax+by+cz+dz+ay+bx+cf+dg+eh+fi+gf+hg+ih+ji+ka+lb+mc+nd+oe+pf+qg+rh+si+tj+uk+vl+xm+yn+zo!p!i!a!l!z!m"));
    ADD_TEST(testDriver("+az+by+cx+dw+av+bu+ct+ds+ar+bq+cp+do+an+bm+cl+dk+aj+bi+ch+dg+af+be+cd+dc+ab+ba+ca+db+ac+bd+ce+df+ag+bh+ci+dj+ak+bl+cm+dn+ax+by+cz+dz+ay+bx+cf+dg+eh+fi+gf+hg+ih+ji+ka+lb+mc+nd+oe+pf+qg+rh+si+tj+uk+vl+xm+yn+zo!k!i!b!s!r!g"));
    ADD_TEST(testDriver("+zg+yh+xq+wi+vn+ud+tp+sy+rf+qg+pq+oh+na+mg+ld+kc+jd+ie+hj+gh+fe+en+ds+cf+br+ad*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!a!b!c!d!e!f!g!h!i!j!k!l!m!n!o!p!q!r!s!t!u!v!w!x!y!z"));
    ADD_TEST(testDriver("+zj+yd+xb+ew+fv+us+ty+sf+rw+qf+pg+oe+nd+mq+ld+kg+jd+iq+hd+ge+fd+ef+dw+cf+bv+aw*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!z!y!x!w!v!u!t!s!r!q!p!o!n!m!l!k!j!i!h!g!f!e!d!c!b!a"));
    ADD_TEST(testDriver("+aj+bf+cb+dr+af+bh+cw+dd+ah+br+cd+dq+ag+bc+cr+df+aq+bh+cr+ds+an+br+cj+dq+aj+bq+cf+dr+af+bv+cc+ds+ab+br+cn+df+aq+bn+cc+dh+aw+bc+ch+de+ar+bb+cq+dn+er+fn+gc+ht+iw+jg+kd+lv+mr+nq+og+pr+qw+rb+st+tj+uu+vn+xd+yv+zt*a*b*c*d*e*f*g*h*i*j*k*l*m*n*o*p*q*r*s*t*u*v*w*x*y*z!a!b!c!d!e!f!g!h!i!j!k!l!m!n!o!p!q!r!s!t!u!v!w!x!y!z"));
    ADD_TEST(testDriver("+dj+ds+mt+wn@d@m@w@a@g@q"));
    ADD_TEST(testDriver("+da+da+da+da+da+da+da+da+da+da+fa+fa+fa+fa+fa+fa+fa+ha+ha+ia+ja+ka+la+ma+oa+pa+qa+ra+za@a@b@c@d@e@f@g@h@i@j@k@l@m@n@o@p@q@r@s@t@u@v@w@x@y@z"));
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

