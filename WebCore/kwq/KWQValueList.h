/*
 * Copyright (C) 2001 Apple Computer, Inc.  All rights reserved.
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

#ifndef QVALUELIST_H_
#define QVALUELIST_H_

template<class T> class QValueListIterator {
public: 
    bool operator!=(const QValueListIterator<T>& it);
    T& operator*();
    const T& operator*() const;
    QValueListIterator<T>& operator++();
    QValueListIterator<T>& operator++(int);
    QValueListIterator<T>& operator--();
};

template<class T> class QValueListConstIterator {
public:
    QValueListConstIterator();
    QValueListConstIterator(const QValueListIterator<T>& it);
    QValueListConstIterator operator++();
    QValueListConstIterator operator++(int);
    bool operator!=(const QValueListConstIterator<T>& it);
    T& operator*();
};

template <class T> class QValueList {
public:
    typedef QValueListIterator<T> Iterator;
    typedef QValueListConstIterator<T> ConstIterator;

    void clear();
    uint count() const;
    bool isEmpty() const;
    void append(const T&);
    void remove(const T&);
    uint contains(const T&);
    Iterator remove(Iterator);
    Iterator fromLast();
    const T& first() const;
    const T& last() const;
    Iterator begin();
    Iterator end();
    ConstIterator begin() const;
    ConstIterator end() const;
    T& operator[] (uint);
    const T& operator[] (uint) const;
};

#endif
