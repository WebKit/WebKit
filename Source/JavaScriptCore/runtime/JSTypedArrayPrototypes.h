/*
 * Copyright (C) 2013 Apple Inc. All rights reserved.
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

#pragma once

#include "JSGenericTypedArrayViewPrototype.h"
#include "JSTypedArrays.h"

namespace JSC {

using JSInt8ArrayPrototype = JSGenericTypedArrayViewPrototype<JSInt8Array>;
using JSInt16ArrayPrototype = JSGenericTypedArrayViewPrototype<JSInt16Array>;
using JSInt32ArrayPrototype = JSGenericTypedArrayViewPrototype<JSInt32Array>;
using JSUint8ArrayPrototype = JSGenericTypedArrayViewPrototype<JSUint8Array>;
using JSUint8ClampedArrayPrototype = JSGenericTypedArrayViewPrototype<JSUint8ClampedArray>;
using JSUint16ArrayPrototype = JSGenericTypedArrayViewPrototype<JSUint16Array>;
using JSUint32ArrayPrototype = JSGenericTypedArrayViewPrototype<JSUint32Array>;
using JSFloat16ArrayPrototype = JSGenericTypedArrayViewPrototype<JSFloat16Array>;
using JSFloat32ArrayPrototype = JSGenericTypedArrayViewPrototype<JSFloat32Array>;
using JSFloat64ArrayPrototype = JSGenericTypedArrayViewPrototype<JSFloat64Array>;
using JSBigInt64ArrayPrototype = JSGenericTypedArrayViewPrototype<JSBigInt64Array>;
using JSBigUint64ArrayPrototype = JSGenericTypedArrayViewPrototype<JSBigUint64Array>;

} // namespace JSC
