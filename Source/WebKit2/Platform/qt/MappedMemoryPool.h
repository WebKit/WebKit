/*
 * Copyright (C) 2010 Apple Inc. All rights reserved.
 * Copyright (C) 2010 Nokia Corporation and/or its subsidiary(-ies)
 * Copyright (C) 2010 University of Szeged
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

#ifndef MappedMemoryPool_h
#define MappedMemoryPool_h

#include <QFile>
#include <QObject>
#include <wtf/StdLibExtras.h>
#include <wtf/Vector.h>

namespace WebKit {

class MappedMemoryPool;

struct MappedMemory {

    QString mappedFileName() const
    {
        ASSERT(file);
        ASSERT(mappedBytes);
        return fileName;
    }

    void markFree()
    {
        ASSERT(mappedBytes);
        dataPtr->isFree = true;
    }

    uchar* data() const
    {
        ASSERT(mappedBytes);
        return dataPtr->bytes;
    }

private:
    friend class MappedMemoryPool;

    MappedMemory()
        : file(0)
        , mappedBytes(0)
        , dataSize(0)
    {
    }

    void markUsed() { dataPtr->isFree = false; }

    size_t mapSize() const { return dataSize + sizeof(Data); }
    bool isFree() const { return dataPtr->isFree; }

    struct Data {
        uint32_t isFree; // keep bytes aligned
        uchar bytes[];
    };

    QFile* file;
    QString fileName;
    union {
        uchar* mappedBytes;
        Data* dataPtr;
    };
    size_t dataSize;
};

class MappedMemoryPool : QObject {
    Q_OBJECT
public:
    static MappedMemoryPool* instance();

    MappedMemory* mapMemory(size_t size);
    MappedMemory* mapFile(QString fileName, size_t size);

private:
    MappedMemoryPool() { }
    ~MappedMemoryPool();

    static MappedMemoryPool* theInstance;

    Vector<MappedMemory> m_pool;
};

} // namespace WebKit

#endif // MappedMemoryPool_h
