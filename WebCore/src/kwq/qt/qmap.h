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

#ifndef QMAP_H_
#define QMAP_H_

#include <kwqdef.h>

template<class K, class T> class QMapIterator {
public:
    QMapIterator();
    QMapIterator(const QMapIterator<K,T>& it);

    const K& key() const;
    const T& data() const;

    bool operator!=(const QMapIterator<K,T>&) const;
    T& operator*();
    const T& operator*() const;
    QMapIterator<K,T>& operator++();
};

template<class K, class T> class QMapConstIterator {
public:
    QMapConstIterator();
    QMapConstIterator(const QMapIterator<K,T>&);
    const K& key() const;
    const T& data() const;

    bool operator!=(const QMapConstIterator<K,T>&) const;
    const T &operator*() const;
    QMapConstIterator<K,T>& operator++();
};

template <class K, class T> class QMap {
public:
    typedef QMapIterator<K, T> Iterator;
    typedef QMapConstIterator< K, T> ConstIterator;
    Iterator begin();
    Iterator end();
    ConstIterator begin() const;
    ConstIterator end() const;
    Iterator insert(const K&, const T&);
    ConstIterator find (const K &) const;
    void remove(const K&);
    void clear();
    uint count() const;
    
    T& operator[](const K& k);
};

#endif
