/*
 * Copyright (C) 2012 Apple Inc. All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "compiler/ArrayBoundsClamper.h"

const char* kIntClampBegin = "// BEGIN: Generated code for array bounds clamping\n\n";
const char* kIntClampEnd = "// END: Generated code for array bounds clamping\n\n";
const char* kIntClampDefinition = "int webgl_int_clamp(int value, int minValue, int maxValue) { return ((value < minValue) ? minValue : ((value > maxValue) ? maxValue : value)); }\n\n";

namespace {

class ArrayBoundsClamperMarker : public TIntermTraverser {
public:
    ArrayBoundsClamperMarker()
        : mNeedsClamp(false)
   {
   }

   virtual bool visitBinary(Visit visit, TIntermBinary* node)
   {
       if (node->getOp() == EOpIndexIndirect)
       {
           TIntermTyped* left = node->getLeft();
           if (left->isArray() || left->isVector() || left->isMatrix())
           {
               node->setAddIndexClamp();
               mNeedsClamp = true;
           }
       }
       return true;
   }

    bool GetNeedsClamp() { return mNeedsClamp; }

private:
    bool mNeedsClamp;
};

}  // anonymous namespace

ArrayBoundsClamper::ArrayBoundsClamper()
    : mArrayBoundsClampDefinitionNeeded(false)
{
}

void ArrayBoundsClamper::OutputClampingFunctionDefinition(TInfoSinkBase& out) const
{
    if (!mArrayBoundsClampDefinitionNeeded)
    {
        return;
    }
    out << kIntClampBegin << kIntClampDefinition << kIntClampEnd;
}

void ArrayBoundsClamper::MarkIndirectArrayBoundsForClamping(TIntermNode* root)
{
    ASSERT(root);

    ArrayBoundsClamperMarker clamper;
    root->traverse(&clamper);
    if (clamper.GetNeedsClamp())
    {
        SetArrayBoundsClampDefinitionNeeded();
    }
}

