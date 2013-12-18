/*
 * Copyright (C) 2013 Google Inc. All rights reserved.
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

#ifndef IndexedDB_h
#define IndexedDB_h

#if ENABLE(INDEXED_DATABASE)

namespace WebCore {

namespace IndexedDB {

enum class TransactionMode {
    ReadOnly = 0,
    ReadWrite = 1,
    VersionChange = 2,
};
const unsigned TransactionModeMaximum = 2;

enum class CursorDirection {
    Next = 0,
    NextNoDuplicate = 1,
    Prev = 2,
    PrevNoDuplicate = 3,
};
const unsigned CursorDirectionMaximum = 3;

enum class CursorType {
    KeyAndValue = 0,
    KeyOnly = 1,
};
const unsigned CursorTypeMaximum = 1;

enum class VersionNullness {
    Null,
    NonNull,
};

} // namespace IndexedDB

} // namespace WebCore

#endif // ENABLED(INDEXED_DATABASE)

#endif // IndexedDB_h
