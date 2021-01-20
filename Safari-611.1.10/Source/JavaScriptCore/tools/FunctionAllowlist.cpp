/*
 * Copyright (C) 2014-2020 Apple Inc. All rights reserved.
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

#include "config.h"
#include "FunctionAllowlist.h"

#if ENABLE(JIT)

#include "CodeBlock.h"
#include <stdio.h>
#include <string.h>

namespace JSC {

FunctionAllowlist::FunctionAllowlist(const char* filename)
{
    if (!filename)
        return;

    FILE* f = fopen(filename, "r");
    if (!f) {
        if (errno == ENOENT) {
            m_hasActiveAllowlist = true;
            m_entries.add(filename);
        } else
            dataLogF("Failed to open file %s. Did you add the file-read-data entitlement to WebProcess.sb? Error code: %s\n", filename, strerror(errno));
        return;
    }

    m_hasActiveAllowlist = true;

    char* line;
    char buffer[BUFSIZ];
    while ((line = fgets(buffer, sizeof(buffer), f))) {
        if (strstr(line, "//") == line)
            continue;

        // Get rid of newlines at the ends of the strings.
        size_t length = strlen(line);
        if (line[length - 1] == '\n') {
            line[length - 1] = '\0';
            length--;
        }

        // Skip empty lines.
        if (!length)
            continue;
        
        m_entries.add(String(line, length));
    }

    int result = fclose(f);
    if (result)
        dataLogF("Failed to close file %s: %s\n", filename, strerror(errno));
}

bool FunctionAllowlist::contains(CodeBlock* codeBlock) const
{
    if (!m_hasActiveAllowlist)
        return true;

    if (m_entries.isEmpty())
        return false;

    String name = String::fromUTF8(codeBlock->inferredName());
    if (m_entries.contains(name))
        return true;

    String hash = String::fromUTF8(codeBlock->hashAsStringIfPossible());
    if (m_entries.contains(hash))
        return true;

    return m_entries.contains(name + '#' + hash);
}

} // namespace JSC

#endif // ENABLE(JIT)

