/*
 * Copyright (C) 2014 Apple Inc. All rights reserved.
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

#include "CPUCount.h"
#include "Interpreter.h"
#include <assert.h>
#include <cstddef>
#include <cstdlib>
#include <errno.h>
#include <fcntl.h>
#include <string>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>
#include <vector>

#include "mbmalloc.h"

Interpreter::Interpreter(const char* fileName)
{
    m_fd = open(fileName, O_RDWR, S_IRUSR | S_IWUSR);
    if (m_fd == -1)
        fprintf(stderr, "failed to open\n");

    struct stat buf;
    fstat(m_fd, &buf);

    m_opCount = buf.st_size / sizeof(Op);
    assert(m_opCount * sizeof(Op) == buf.st_size);

    size_t maxSlot = 0;

    std::vector<Op> ops(1024);
    size_t remaining = m_opCount * sizeof(Op);
    while (remaining) {
        size_t bytes = std::min(remaining, ops.size() * sizeof(Op));
        remaining -= bytes;
        read(m_fd, ops.data(), bytes);

        size_t opCount = bytes / sizeof(Op);
        for (size_t i = 0; i < opCount; ++i) {
            Op op = ops[i];
            if (op.slot > maxSlot)
                maxSlot = op.slot;
        }
    }

    m_objects.resize(maxSlot + 1);
}

Interpreter::~Interpreter()
{
    int result = close(m_fd);
    if (result == -1)
        fprintf(stderr, "failed to close\n");
}

void Interpreter::run()
{
    std::vector<Op> ops(1024);
    lseek(m_fd, 0, SEEK_SET);
    size_t remaining = m_opCount * sizeof(Op);
    while (remaining) {
        size_t bytes = std::min(remaining, ops.size() * sizeof(Op));
        remaining -= bytes;
        read(m_fd, ops.data(), bytes);

        size_t opCount = bytes / sizeof(Op);
        for (size_t i = 0; i < opCount; ++i) {
            Op op = ops[i];
            switch (op.opcode) {
            case op_malloc: {
                m_objects[op.slot] = { mbmalloc(op.size), op.size };
                assert(m_objects[op.slot].object);
                bzero(m_objects[op.slot].object, op.size);
                break;
            }
            case op_free: {
                assert(m_objects[op.slot].object);
                assert(m_objects[op.slot].size);
                mbfree(m_objects[op.slot].object, m_objects[op.slot].size);
                m_objects[op.slot] = { 0, 0 };
                break;
            }
            case op_realloc: {
                assert(m_objects[op.slot].object);
                assert(m_objects[op.slot].size);
                m_objects[op.slot] = { mbrealloc(m_objects[op.slot].object, m_objects[op.slot].size, op.size), op.size };
                break;
            }
            default: {
                fprintf(stderr, "bad opcode: %d\n", op.opcode);
                abort();
                break;
            }
            }
        }
    }

    // A recording might not free all of its allocations.
    for (size_t i = 0; i < m_objects.size(); ++i) {
        if (!m_objects[i].object)
            continue;
        mbfree(m_objects[i].object, m_objects[i].size);
        m_objects[i] = { 0, 0 };
    }
}
