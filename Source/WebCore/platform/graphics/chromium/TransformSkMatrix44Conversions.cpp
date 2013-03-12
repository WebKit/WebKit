/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1.  Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 * 2.  Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in the
 *     documentation and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY APPLE INC. AND ITS CONTRIBUTORS ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL APPLE INC. OR ITS CONTRIBUTORS BE LIABLE FOR ANY
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
 * ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include "config.h"

#include "TransformSkMatrix44Conversions.h"

#include "TransformationMatrix.h"

namespace WebCore {

SkMatrix44 TransformSkMatrix44Conversions::convert(const TransformationMatrix& matrix)
{
    SkMatrix44 ret(SkMatrix44::kUninitialized_Constructor);
    ret.setDouble(0, 0, matrix.m11());
    ret.setDouble(0, 1, matrix.m21());
    ret.setDouble(0, 2, matrix.m31());
    ret.setDouble(0, 3, matrix.m41());
    ret.setDouble(1, 0, matrix.m12());
    ret.setDouble(1, 1, matrix.m22());
    ret.setDouble(1, 2, matrix.m32());
    ret.setDouble(1, 3, matrix.m42());
    ret.setDouble(2, 0, matrix.m13());
    ret.setDouble(2, 1, matrix.m23());
    ret.setDouble(2, 2, matrix.m33());
    ret.setDouble(2, 3, matrix.m43());
    ret.setDouble(3, 0, matrix.m14());
    ret.setDouble(3, 1, matrix.m24());
    ret.setDouble(3, 2, matrix.m34());
    ret.setDouble(3, 3, matrix.m44());
    return ret;
}

} // namespace WebCore
