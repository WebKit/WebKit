/*
 * Copyright (C) 2019 Apple Inc. All rights reserved.
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

#import "config.h"
#import "JSScriptInternal.h"

#import "APICast.h"
#import "JSContextInternal.h"
#import "JSValuePrivate.h"
#import "Symbol.h"

#if JSC_OBJC_API_ENABLED

@implementation JSScript {
    String m_source;
}

+ (instancetype)scriptWithSource:(NSString *)source inVirtualMachine:(JSVirtualMachine *)vm
{
    UNUSED_PARAM(vm);
    JSScript *result = [[JSScript alloc] init];
    result->m_source = source;
    return result;
}

template<typename Vector>
static bool fillBufferWithContentsOfFile(FILE* file, Vector& buffer)
{
    // We might have injected "use strict"; at the top.
    size_t initialSize = buffer.size();
    if (fseek(file, 0, SEEK_END) == -1)
        return false;
    long bufferCapacity = ftell(file);
    if (bufferCapacity == -1)
        return false;
    if (fseek(file, 0, SEEK_SET) == -1)
        return false;
    buffer.resize(bufferCapacity + initialSize);
    size_t readSize = fread(buffer.data() + initialSize, 1, buffer.size(), file);
    return readSize == buffer.size() - initialSize;
}

static bool fillBufferWithContentsOfFile(const String& fileName, Vector<char>& buffer)
{
    FILE* f = fopen(fileName.utf8().data(), "rb");
    if (!f) {
        fprintf(stderr, "Could not open file: %s\n", fileName.utf8().data());
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    fclose(f);

    return result;
}


+ (instancetype)scriptFromUTF8File:(NSURL *)filePath inVirtualMachine:(JSVirtualMachine *)vm withCodeSigning:(NSURL *)codeSigningPath andBytecodeCache:(NSURL *)cachePath
{
    // FIXME: This should check codeSigning.
    UNUSED_PARAM(codeSigningPath);
    // FIXME: This should actually cache bytecode.
    UNUSED_PARAM(cachePath);
    UNUSED_PARAM(vm);
    URL filePathURL([filePath absoluteURL]);
    if (!filePathURL.isLocalFile())
        return nil;
    // FIXME: This should mmap the contents of the file instead of copying it into dirty memory.
    Vector<char> buffer;
    if (!fillBufferWithContentsOfFile(filePathURL.fileSystemPath(), buffer))
        return nil;

    JSScript *result = [[JSScript alloc] init];
    result->m_source = String::fromUTF8WithLatin1Fallback(buffer.data(), buffer.size());
    return result;
}

const String& getJSScriptSourceCode(JSScript *module) { return module->m_source; }

@end


#endif
