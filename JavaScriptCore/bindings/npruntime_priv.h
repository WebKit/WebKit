/*
 * Copyright (C) 2003 Apple Computer, Inc.  All rights reserved.
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
 * THIS SOFTWARE IS PROVIDED BY APPLE COMPUTER, INC. ``AS IS'' AND ANY
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL APPLE COMPUTER, INC. OR
 * CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 * EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 * PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 * OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE. 
 */
#ifndef _NP_RUNTIME_PRIV_H_
#define _NP_RUNTIME_PRIV_H_

NPBool NPN_VariantIsVoid (const NPVariant *variant);
NPBool NPN_VariantIsNull (const NPVariant *variant);
NPBool NPN_VariantIsUndefined (const NPVariant *variant);
NPBool NPN_VariantIsBool (const NPVariant *variant);
NPBool NPN_VariantIsInt32 (const NPVariant *variant);
NPBool NPN_VariantIsDouble (const NPVariant *variant);
NPBool NPN_VariantIsString (const NPVariant *variant);
NPBool NPN_VariantIsObject (const NPVariant *variant);
NPBool NPN_VariantToBool (const NPVariant *variant, NPBool *result);
NPBool NPN_VariantToInt32 (const NPVariant *variant, int32_t *result);
NPBool NPN_VariantToDouble (const NPVariant *variant, double *result);
NPString NPN_VariantToString (const NPVariant *variant);
NPString NPN_VariantToStringCopy (const NPVariant *variant);
NPBool NPN_VariantToObject (const NPVariant *variant, NPObject **result);

/*
    NPVariants initialized with the NPN_InitializeVariantXXX() functions
    must be released using the NPN_ReleaseVariantValue() function.
*/
void NPN_InitializeVariantAsVoid (NPVariant *variant);
void NPN_InitializeVariantAsNull (NPVariant *variant);
void NPN_InitializeVariantAsUndefined (NPVariant *variant);
void NPN_InitializeVariantWithBool (NPVariant *variant, NPBool value);
void NPN_InitializeVariantWithInt32 (NPVariant *variant, int32_t value);
void NPN_InitializeVariantWithDouble (NPVariant *variant, double value);

/*
    NPN_InitializeVariantWithString() does not copy string data.  However
    the string data will be deallocated by calls to NPReleaseVariantValue().
*/
void NPN_InitializeVariantWithString (NPVariant *variant, const NPString *value);

/*
    NPN_InitializeVariantWithStringCopy() will copy string data.  The string data
    will be deallocated by calls to NPReleaseVariantValue().
*/
void NPN_InitializeVariantWithStringCopy (NPVariant *variant, const NPString *value);

/*
    NPN_InitializeVariantWithObject() retains the NPObject.  The object will be released
    by calls to NPReleaseVariantValue();
*/
void NPN_InitializeVariantWithObject (NPVariant *variant, NPObject *value);

void NPN_InitializeVariantWithVariant (NPVariant *destination, const NPVariant *source);

#endif
