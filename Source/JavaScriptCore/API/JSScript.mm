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
#import "Identifier.h"
#import "JSContextInternal.h"
#import "JSScriptSourceProvider.h"
#import "JSSourceCode.h"
#import "JSValuePrivate.h"
#import "JSVirtualMachineInternal.h"
#import "ParserError.h"
#import "Symbol.h"
#include <sys/stat.h>

#if JSC_OBJC_API_ENABLED

@implementation JSScript {
    __weak JSVirtualMachine* m_virtualMachine;
    String m_source;
    NSURL* m_cachePath;
    JSC::CachedBytecode m_cachedBytecode;
    JSC::Strong<JSC::JSSourceCode> m_jsSourceCode;
    UniquedStringImpl* m_moduleKey;
}

+ (instancetype)scriptWithSource:(NSString *)source inVirtualMachine:(JSVirtualMachine *)vm
{
    JSScript *result = [[[JSScript alloc] init] autorelease];
    result->m_source = source;
    result->m_virtualMachine = vm;
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

static bool fillBufferWithContentsOfFile(const String& fileName, Vector<LChar>& buffer)
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


+ (instancetype)scriptFromASCIIFile:(NSURL *)filePath inVirtualMachine:(JSVirtualMachine *)vm withCodeSigning:(NSURL *)codeSigningPath andBytecodeCache:(NSURL *)cachePath
{
    // FIXME: This should check codeSigning.
    UNUSED_PARAM(codeSigningPath);
    URL filePathURL([filePath absoluteURL]);
    if (!filePathURL.isLocalFile())
        return nil;
    // FIXME: This should mmap the contents of the file instead of copying it into dirty memory.
    Vector<LChar> buffer;
    if (!fillBufferWithContentsOfFile(filePathURL.fileSystemPath(), buffer))
        return nil;

    if (!charactersAreAllASCII(buffer.data(), buffer.size()))
        return nil;

    JSScript *result = [[[JSScript alloc] init] autorelease];
    result->m_virtualMachine = vm;
    result->m_source = String::fromUTF8WithLatin1Fallback(buffer.data(), buffer.size());
    result->m_cachePath = cachePath;
    [result readCache];
    return result;
}

+ (instancetype)scriptFromUTF8File:(NSURL *)filePath inVirtualMachine:(JSVirtualMachine *)vm withCodeSigning:(NSURL *)codeSigningPath andBytecodeCache:(NSURL *)cachePath
{
    return [JSScript scriptFromASCIIFile:filePath inVirtualMachine:vm withCodeSigning:codeSigningPath andBytecodeCache:cachePath];
}

- (void)dealloc
{
    if (m_cachedBytecode.size() && !m_cachedBytecode.owned())
        munmap(const_cast<void*>(m_cachedBytecode.data()), m_cachedBytecode.size());
    [super dealloc];
}

- (void)readCache
{
    if (!m_cachePath)
        return;

    int fd = open(m_cachePath.path.UTF8String, O_RDONLY);
    if (fd == -1)
        return;

    int rc = flock(fd, LOCK_SH | LOCK_NB);
    if (rc) {
        close(fd);
        return;
    }

    struct stat sb;
    int res = fstat(fd, &sb);
    size_t size = static_cast<size_t>(sb.st_size);
    if (res || !size) {
        close(fd);
        return;
    }

    void* buffer = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, fd, 0);
    close(fd);

    m_cachedBytecode = JSC::CachedBytecode { buffer, size };
}

- (void)writeCache
{
    if (m_cachedBytecode.size() || !m_cachePath)
        return;

    JSC::ParserError error;
    m_cachedBytecode = JSC::generateModuleBytecode(m_virtualMachine.vm, m_jsSourceCode->sourceCode(), error);
    if (error.isValid())
        return;
    int fd = open(m_cachePath.path.UTF8String, O_CREAT | O_WRONLY, 0666);
    if (fd == -1)
        return;
    int rc = flock(fd, LOCK_EX | LOCK_NB);
    if (!rc)
        write(fd, m_cachedBytecode.data(), m_cachedBytecode.size());
    close(fd);
}

@end

@implementation JSScript(Internal)

- (unsigned)hash
{
    return m_source.hash();
}

- (const String&)source
{
    return m_source;
}

- (const JSC::CachedBytecode*)cachedBytecode
{
    return &m_cachedBytecode;
}

- (JSC::JSSourceCode*)jsSourceCode:(const JSC::Identifier&)moduleKey
{
    if (m_jsSourceCode) {
        ASSERT(moduleKey.impl() == m_moduleKey);
        return m_jsSourceCode.get();
    }

    JSC::VM& vm = m_virtualMachine.vm;
    TextPosition startPosition { };
    Ref<JSScriptSourceProvider> sourceProvider = JSScriptSourceProvider::create(self, JSC::SourceOrigin(moduleKey.string()), URL({ }, moduleKey.string()), TextPosition(), JSC::SourceProviderSourceType::Module);
    JSC::SourceCode sourceCode(WTFMove(sourceProvider), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
    JSC::JSSourceCode* jsSourceCode = JSC::JSSourceCode::create(vm, WTFMove(sourceCode));
    m_jsSourceCode.set(vm, jsSourceCode);
    [self writeCache];
    return jsSourceCode;
}

@end


#endif
