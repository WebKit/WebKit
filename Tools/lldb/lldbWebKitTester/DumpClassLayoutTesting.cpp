/*
 * Copyright (C) 2018 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS''
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS
 * BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF
 * THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "DumpClassLayoutTesting.h"

#include <memory>

/*
*** Dumping AST Record Layout
         0 | class BasicClassLayout
         0 |   int intMember
         4 |   _Bool boolMember
           | [sizeof=8, dsize=5, align=4,
           |  nvsize=5, nvalign=4]
*/
class BasicClassLayout {
    int intMember;
    bool boolMember;
};

/*
*** Dumping AST Record Layout
         0 | class PaddingBetweenClassMembers
         0 |   class BasicClassLayout basic1
         0 |     int intMember
         4 |     _Bool boolMember
         8 |   class BasicClassLayout basic2
         8 |     int intMember
        12 |     _Bool boolMember
           | [sizeof=16, dsize=16, align=4,
           |  nvsize=16, nvalign=4]
*/
class PaddingBetweenClassMembers {
    BasicClassLayout basic1;
    BasicClassLayout basic2;
};

/*
*** Dumping AST Record Layout
         0 | class BoolMemberFirst
         0 |   _Bool boolMember
         4 |   int intMember
           | [sizeof=8, dsize=8, align=4,
           |  nvsize=8, nvalign=4]
*/
class BoolMemberFirst {
    bool boolMember;
    int intMember;
};

/*
*** Dumping AST Record Layout
         0 | class BoolPaddingClass
         0 |   _Bool bool1
         1 |   _Bool bool2
         2 |   _Bool bool3
         4 |   class BoolMemberFirst memberClass
         4 |     _Bool boolMember
         8 |     int intMember
           | [sizeof=12, dsize=12, align=4,
           |  nvsize=12, nvalign=4]
*/
class BoolPaddingClass {
    bool bool1;
    bool bool2;
    bool bool3;
    BoolMemberFirst memberClass;
};

/*
*** Dumping AST Record Layout
         0 | class EmptyClass (empty)
           | [sizeof=1, dsize=1, align=1,
           |  nvsize=1, nvalign=1]
*/
class EmptyClass {
    void doStuff() { }
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithEmptyClassMembers
         0 |   int intMember
         4 |   class EmptyClass empty1 (empty)
         5 |   _Bool boolMember
         6 |   class EmptyClass empty2 (empty)
         8 |   int intMember2
           | [sizeof=12, dsize=12, align=4,
           |  nvsize=12, nvalign=4]
*/
class ClassWithEmptyClassMembers {
    int intMember;
    EmptyClass empty1;
    bool boolMember;
    EmptyClass empty2;
    int intMember2;
};

/*
*** Dumping AST Record Layout
         0 | class VirtualBaseClass
         0 |   (VirtualBaseClass vtable pointer)
           | [sizeof=8, dsize=8, align=8,
           |  nvsize=8, nvalign=8]
*/
class VirtualBaseClass {
public:
    virtual ~VirtualBaseClass() { }
};

/*
*** Dumping AST Record Layout
         0 | class VirtualBaseClass2
         0 |   (VirtualBaseClass2 vtable pointer)
           | [sizeof=8, dsize=8, align=8,
           |  nvsize=8, nvalign=8]
*/
class VirtualBaseClass2 {
public:
    virtual ~VirtualBaseClass2() { }
};

/*
*** Dumping AST Record Layout
         0 | class SimpleVirtualClass
         0 |   (SimpleVirtualClass vtable pointer)
         8 |   int intMember
        16 |   double doubleMember
           | [sizeof=24, dsize=24, align=8,
           |  nvsize=24, nvalign=8]
*/
class SimpleVirtualClass {
public:
    virtual ~SimpleVirtualClass() { }
    virtual void doStuff() { }
    int intMember;
    double doubleMember;
};

/*
*** Dumping AST Record Layout
         0 | class VirtualClassWithNonVirtualBase
         0 |   (VirtualClassWithNonVirtualBase vtable pointer)
         8 |   class BasicClassLayout (base)
         8 |     int intMember
        12 |     _Bool boolMember
        16 |   double doubleMember
           | [sizeof=24, dsize=24, align=8,
           |  nvsize=24, nvalign=8]
*/
class VirtualClassWithNonVirtualBase : public BasicClassLayout  {
public:
    virtual ~VirtualClassWithNonVirtualBase() { }
    virtual void doStuff() { }
    double doubleMember;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithVirtualBase
         0 |   class VirtualBaseClass (primary base)
         0 |     (VirtualBaseClass vtable pointer)
         8 |   _Bool boolMember
           | [sizeof=16, dsize=9, align=8,
           |  nvsize=9, nvalign=8]
*/
class ClassWithVirtualBase: public VirtualBaseClass {
public:
    ClassWithVirtualBase() { }
    virtual void doStuff() { }
    bool boolMember;
};

/*
*** Dumping AST Record Layout
         0 | class InterleavedVirtualNonVirtual
         0 |   class ClassWithVirtualBase (primary base)
         0 |     class VirtualBaseClass (primary base)
         0 |       (VirtualBaseClass vtable pointer)
         8 |     _Bool boolMember
         9 |   _Bool boolMember
           | [sizeof=16, dsize=10, align=8,
           |  nvsize=10, nvalign=8]
*/
class InterleavedVirtualNonVirtual: public ClassWithVirtualBase {
public:
    InterleavedVirtualNonVirtual() { }
    virtual ~InterleavedVirtualNonVirtual() { }
    bool boolMember;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithTwoVirtualBaseClasses
         0 |   class VirtualBaseClass (primary base)
         0 |     (VirtualBaseClass vtable pointer)
         8 |   class VirtualBaseClass2 (base)
         8 |     (VirtualBaseClass2 vtable pointer)
        16 |   _Bool boolMember
           | [sizeof=24, dsize=17, align=8,
           |  nvsize=17, nvalign=8]
*/
class ClassWithTwoVirtualBaseClasses: public VirtualBaseClass, VirtualBaseClass2 {
public:
    ClassWithTwoVirtualBaseClasses() { }
    bool boolMember;
};

/*
*** Dumping AST Record Layout
         0 | class VirtualBase
         0 |   (VirtualBase vtable pointer)
         8 |   _Bool baseMember
           | [sizeof=16, dsize=9, align=8,
           |  nvsize=9, nvalign=8]
*/
class VirtualBase {
public:
    virtual ~VirtualBase() { }
    bool baseMember;
};

/*
*** Dumping AST Record Layout
         0 | class VirtualInheritingA
         0 |   (VirtualInheritingA vtable pointer)
         8 |   int intMemberA
        16 |   class VirtualBase (virtual base)
        16 |     (VirtualBase vtable pointer)
        24 |     _Bool baseMember
           | [sizeof=32, dsize=25, align=8,
           |  nvsize=12, nvalign=8]
*/
class VirtualInheritingA: virtual public VirtualBase {
public:
    VirtualInheritingA() { }
    int intMemberA;
};

/*
*** Dumping AST Record Layout
         0 | class VirtualInheritingB
         0 |   (VirtualInheritingB vtable pointer)
         8 |   class BasicClassLayout (base)
         8 |     int intMember
        12 |     _Bool boolMember
        16 |   int intMemberB
        24 |   class VirtualBase (virtual base)
        24 |     (VirtualBase vtable pointer)
        32 |     _Bool baseMember
           | [sizeof=40, dsize=33, align=8,
           |  nvsize=20, nvalign=8]
*/
class VirtualInheritingB: public virtual VirtualBase, public BasicClassLayout {
public:
    VirtualInheritingB() { }
    int intMemberB;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithVirtualInheritance
         0 |   class VirtualInheritingA (primary base)
         0 |     (VirtualInheritingA vtable pointer)
         8 |     int intMemberA
        16 |   class VirtualInheritingB (base)
        16 |     (VirtualInheritingB vtable pointer)
        24 |     class BasicClassLayout (base)
        24 |       int intMember
        28 |       _Bool boolMember
        32 |     int intMemberB
        40 |   double derivedMember
        48 |   class VirtualBase (virtual base)
        48 |     (VirtualBase vtable pointer)
        56 |     _Bool baseMember
           | [sizeof=64, dsize=57, align=8,
           |  nvsize=48, nvalign=8]
*/
class ClassWithVirtualInheritance : public VirtualInheritingA, public VirtualInheritingB {
public:
    double derivedMember;
};

/*
*** Dumping AST Record Layout
         0 | class DerivedClassWithIndirectVirtualInheritance
         0 |   class ClassWithVirtualInheritance (primary base)
         0 |     class VirtualInheritingA (primary base)
         0 |       (VirtualInheritingA vtable pointer)
         8 |       int intMemberA
        16 |     class VirtualInheritingB (base)
        16 |       (VirtualInheritingB vtable pointer)
        24 |       class BasicClassLayout (base)
        24 |         int intMember
        28 |         _Bool boolMember
        32 |       int intMemberB
        40 |     double derivedMember
        48 |   long mostDerivedMember
        56 |   class VirtualBase (virtual base)
        56 |     (VirtualBase vtable pointer)
        64 |     _Bool baseMember
           | [sizeof=72, dsize=65, align=8,
           |  nvsize=56, nvalign=8]
*/
class DerivedClassWithIndirectVirtualInheritance : public ClassWithVirtualInheritance {
public:
    long mostDerivedMember;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithInheritanceAndClassMember
         0 |   class VirtualInheritingA (primary base)
         0 |     (VirtualInheritingA vtable pointer)
         8 |     int intMemberA
        16 |   class VirtualInheritingB dataMember
        16 |     (VirtualInheritingB vtable pointer)
        24 |     class BasicClassLayout (base)
        24 |       int intMember
        28 |       _Bool boolMember
        32 |     int intMemberB
        40 |     class VirtualBase (virtual base)
        40 |       (VirtualBase vtable pointer)
        48 |       _Bool baseMember
        56 |   double derivedMember
        64 |   class VirtualBase (virtual base)
        64 |     (VirtualBase vtable pointer)
        72 |     _Bool baseMember
           | [sizeof=80, dsize=73, align=8,
           |  nvsize=64, nvalign=8]
*/
class ClassWithInheritanceAndClassMember : public VirtualInheritingA {
public:
    VirtualInheritingB dataMember;
    double derivedMember;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithClassMembers
         0 |   _Bool boolMember
         4 |   class BasicClassLayout classMember
         4 |     int intMember
         8 |     _Bool boolMember
        16 |   class ClassWithTwoVirtualBaseClasses virtualClassesMember
        16 |     class VirtualBaseClass (primary base)
        16 |       (VirtualBaseClass vtable pointer)
        24 |     class VirtualBaseClass2 (base)
        24 |       (VirtualBaseClass2 vtable pointer)
        32 |     _Bool boolMember
        40 |   double doubleMember
        48 |   class ClassWithVirtualBase virtualClassMember
        48 |     class VirtualBaseClass (primary base)
        48 |       (VirtualBaseClass vtable pointer)
        56 |     _Bool boolMember
        64 |   int intMember
           | [sizeof=72, dsize=68, align=8,
           |  nvsize=68, nvalign=8]

*/
class ClassWithClassMembers {
    bool boolMember;
    BasicClassLayout classMember;
    ClassWithTwoVirtualBaseClasses virtualClassesMember;
    double doubleMember;
    ClassWithVirtualBase virtualClassMember;
    int intMember;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithPointerMember
         0 |   _Bool boolMember
         8 |   class BasicClassLayout * classMemberPointer
        16 |   int intMember
           | [sizeof=24, dsize=20, align=8,
           |  nvsize=20, nvalign=8]
*/
class ClassWithPointerMember {
    bool boolMember;
    BasicClassLayout* classMemberPointer;
    int intMember;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithBitfields
         0 |   _Bool boolMember
     1:0-0 |   unsigned int bitfield1
     1:1-2 |   unsigned int bitfield2
     1:3-3 |   unsigned int bitfield3
     1:4-4 |   _Bool bitfield4
     1:5-6 |   _Bool bitfield5
     1:7-7 |   _Bool bitfield6
         4 |   int intMember
     8:0-0 |   unsigned int bitfield7
     8:1-1 |   _Bool bitfield8
           | [sizeof=12, dsize=9, align=4,
           |  nvsize=9, nvalign=4]
*/
class ClassWithBitfields {
    bool boolMember;

    unsigned bitfield1 : 1;
    unsigned bitfield2 : 2;
    unsigned bitfield3 : 1;
    bool bitfield4 : 1;
    bool bitfield5 : 2;
    bool bitfield6 : 1;

    int intMember;

    unsigned bitfield7 : 1;
    bool bitfield8 : 1;
};

/*
*** Dumping AST Record Layout
         0 | class ClassWithPaddedBitfields
         0 |   _Bool boolMember
     1:0-0 |   unsigned int bitfield1
     1:1-1 |   _Bool bitfield2
     1:2-3 |   unsigned int bitfield3
     1:4-4 |   unsigned int bitfield4
     1:5-6 |   unsigned long bitfield5
         4 |   int intMember
     8:0-0 |   unsigned int bitfield7
     8:1-9 |   unsigned int bitfield8
     9:2-2 |   _Bool bitfield9
           | [sizeof=16, dsize=10, align=8,
           |  nvsize=10, nvalign=8]
*/
class ClassWithPaddedBitfields {
    bool boolMember;

    unsigned bitfield1 : 1;
    bool bitfield2 : 1;
    unsigned bitfield3 : 2;
    unsigned bitfield4 : 1;
    unsigned long bitfield5: 2;

    int intMember;

    unsigned bitfield7 : 1;
    unsigned bitfield8 : 9;
    bool bitfield9 : 1;
};

/*
*** Dumping AST Record Layout
         0 | class MemberHasBitfieldPadding
         0 |   class ClassWithPaddedBitfields bitfieldMember
         0 |     _Bool boolMember
     1:0-0 |     unsigned int bitfield1
     1:1-1 |     _Bool bitfield2
     1:2-3 |     unsigned int bitfield3
     1:4-4 |     unsigned int bitfield4
     1:5-6 |     unsigned long bitfield5
         4 |     int intMember
     8:0-0 |     unsigned int bitfield7
     8:1-9 |     unsigned int bitfield8
     9:2-2 |     _Bool bitfield9
    16:0-0 |   _Bool bitfield1
           | [sizeof=24, dsize=17, align=8,
           |  nvsize=17, nvalign=8]
*/
class MemberHasBitfieldPadding {
    ClassWithPaddedBitfields bitfieldMember;
    bool bitfield1 : 1;
};

/*
*** Dumping AST Record Layout
         0 | class InheritsFromClassWithPaddedBitfields
         0 |   class ClassWithPaddedBitfields (base)
         0 |     _Bool boolMember
     1:0-0 |     unsigned int bitfield1
     1:1-1 |     _Bool bitfield2
     1:2-3 |     unsigned int bitfield3
     1:4-4 |     unsigned int bitfield4
     1:5-6 |     unsigned long bitfield5
         4 |     int intMember
     8:0-0 |     unsigned int bitfield7
     8:1-9 |     unsigned int bitfield8
     9:2-2 |     _Bool bitfield9
    10:0-0 |   _Bool derivedBitfield
           | [sizeof=16, dsize=11, align=8,
           |  nvsize=11, nvalign=8]
*/
class InheritsFromClassWithPaddedBitfields : public ClassWithPaddedBitfields {
    bool derivedBitfield : 1;
};

void avoidClassDeadStripping()
{
    BasicClassLayout basicClassInstance;
    BoolPaddingClass boolPaddingClassInstance;
    ClassWithEmptyClassMembers classWithEmptyClassMembersInstance;
    PaddingBetweenClassMembers paddingBetweenClassMembersInstance;
    SimpleVirtualClass simpleVirtualClassInstance;
    InterleavedVirtualNonVirtual interleavedVirtualNonVirtualInstance;
    VirtualClassWithNonVirtualBase virtualClassWithNonVirtualBaseInstance;
    ClassWithVirtualBase classWithVirtualBaseInstance;
    ClassWithVirtualInheritance classWithVirtualInheritanceInstance;
    DerivedClassWithIndirectVirtualInheritance derivedClassWithIndirectVirtualInheritanceInstance;
    ClassWithInheritanceAndClassMember classWithInheritanceAndClassMemberInstance;
    ClassWithClassMembers classWithClassMembersInstance;
    ClassWithPointerMember classWithPointerMemberInstance;
    ClassWithBitfields classWithBitfieldsInstance;
    ClassWithPaddedBitfields classWithPaddedBitfieldsInstance;
    MemberHasBitfieldPadding memberHasBitfieldPaddingInstance;
    InheritsFromClassWithPaddedBitfields inheritsFromClassWithPaddedBitfieldsInstance;
}
