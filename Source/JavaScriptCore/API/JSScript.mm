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
#import "CachedTypes.h"
#import "CodeCache.h"
#import "Identifier.h"
#import "JSContextInternal.h"
#import "JSScriptSourceProvider.h"
#import "JSSourceCode.h"
#import "JSValuePrivate.h"
#import "JSVirtualMachineInternal.h"
#import "ParserError.h"
#import "Symbol.h"
#include <sys/stat.h>
#include <wtf/FileSystem.h>

#if JSC_OBJC_API_ENABLED

@implementation JSScript {
    __weak JSVirtualMachine* m_virtualMachine;
    JSScriptType m_type;
    FileSystem::MappedFileData m_mappedSource;
    String m_source;
    RetainPtr<NSURL> m_sourceURL;
    RetainPtr<NSURL> m_cachePath;
    JSC::CachedBytecode m_cachedBytecode;
    JSC::Strong<JSC::JSSourceCode> m_jsSourceCode;
    int m_cacheFileDescriptor;
}

+ (instancetype)scriptWithSource:(NSString *)source inVirtualMachine:(JSVirtualMachine *)vm
{
    JSScript *result = [[[JSScript alloc] init] autorelease];
    result->m_source = source;
    result->m_virtualMachine = vm;
    result->m_type = kJSScriptTypeModule;
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
    UNUSED_PARAM(codeSigningPath);
    UNUSED_PARAM(cachePath);

    URL filePathURL([filePath absoluteURL]);
    if (!filePathURL.isLocalFile())
        return nil;

    Vector<LChar> buffer;
    if (!fillBufferWithContentsOfFile(filePathURL.fileSystemPath(), buffer))
        return nil;

    if (!charactersAreAllASCII(buffer.data(), buffer.size()))
        return nil;

    JSScript *result = [[[JSScript alloc] init] autorelease];
    result->m_virtualMachine = vm;
    result->m_source = String::fromUTF8WithLatin1Fallback(buffer.data(), buffer.size());
    result->m_type = kJSScriptTypeModule;
    return result;
}

+ (instancetype)scriptFromUTF8File:(NSURL *)filePath inVirtualMachine:(JSVirtualMachine *)vm withCodeSigning:(NSURL *)codeSigningPath andBytecodeCache:(NSURL *)cachePath
{
    return [JSScript scriptFromASCIIFile:filePath inVirtualMachine:vm withCodeSigning:codeSigningPath andBytecodeCache:cachePath];
}

static JSScript *createError(NSString *message, NSError** error)
{
    if (error)
        *error = [NSError errorWithDomain:@"JSScriptErrorDomain" code:1 userInfo:@{ @"message": message }];
    return nil;
}

+ (instancetype)scriptOfType:(JSScriptType)type withSource:(NSString *)source andSourceURL:(NSURL *)sourceURL andBytecodeCache:(NSURL *)cachePath inVirtualMachine:(JSVirtualMachine *)vm error:(out NSError **)error
{
    UNUSED_PARAM(error);
    JSScript *result = [[[JSScript alloc] init] autorelease];
    result->m_virtualMachine = vm;
    result->m_type = type;
    result->m_source = source;
    result->m_sourceURL = sourceURL;
    result->m_cachePath = cachePath;
    [result readCache];
    return result;
}

+ (instancetype)scriptOfType:(JSScriptType)type memoryMappedFromASCIIFile:(NSURL *)filePath withSourceURL:(NSURL *)sourceURL andBytecodeCache:(NSURL *)cachePath inVirtualMachine:(JSVirtualMachine *)vm error:(out NSError **)error
{
    UNUSED_PARAM(error);
    URL filePathURL([filePath absoluteURL]);
    if (!filePathURL.isLocalFile())
        return createError([NSString stringWithFormat:@"File path %@ is not a local file", static_cast<NSString *>(filePathURL)], error);

    bool success = false;
    String systemPath = filePathURL.fileSystemPath();
    FileSystem::MappedFileData fileData(systemPath, success);
    if (!success)
        return createError([NSString stringWithFormat:@"File at path %@ could not be mapped.", static_cast<NSString *>(systemPath)], error);

    if (!charactersAreAllASCII(reinterpret_cast<const LChar*>(fileData.data()), fileData.size()))
        return createError([NSString stringWithFormat:@"Not all characters in file at %@ are ASCII.", static_cast<NSString *>(systemPath)], error);

    JSScript *result = [[[JSScript alloc] init] autorelease];
    result->m_virtualMachine = vm;
    result->m_type = type;
    result->m_source = String(StringImpl::createWithoutCopying(bitwise_cast<const LChar*>(fileData.data()), fileData.size()));
    result->m_mappedSource = WTFMove(fileData);
    result->m_sourceURL = sourceURL;
    result->m_cachePath = cachePath;
    [result readCache];
    return result;
}

- (void)dealloc
{
    if (m_cachedBytecode.size() && !m_cachedBytecode.owned())
        munmap(const_cast<void*>(m_cachedBytecode.data()), m_cachedBytecode.size());

    if (m_cacheFileDescriptor != -1)
        close(m_cacheFileDescriptor);

    [super dealloc];
}

- (void)readCache
{
    if (!m_cachePath)
        return;

    m_cacheFileDescriptor = open([m_cachePath path].UTF8String, O_CREAT | O_RDWR | O_EXLOCK | O_NONBLOCK, 0666);
    if (m_cacheFileDescriptor == -1)
        return;

    struct stat sb;
    int res = fstat(m_cacheFileDescriptor, &sb);
    size_t size = static_cast<size_t>(sb.st_size);
    if (res || !size)
        return;

    void* buffer = mmap(nullptr, size, PROT_READ, MAP_PRIVATE, m_cacheFileDescriptor, 0);

    JSC::CachedBytecode cachedBytecode { buffer, size };

    JSC::VM& vm = m_virtualMachine.vm;
    const JSC::SourceCode& sourceCode = [self jsSourceCode]->sourceCode();
    JSC::SourceCodeKey key = m_type == kJSScriptTypeProgram ? sourceCodeKeyForSerializedProgram(vm, sourceCode) : sourceCodeKeyForSerializedModule(vm, sourceCode);
    if (isCachedBytecodeStillValid(vm, cachedBytecode, key, m_type == kJSScriptTypeProgram ? JSC::SourceCodeType::ProgramType : JSC::SourceCodeType::ModuleType))
        m_cachedBytecode = WTFMove(cachedBytecode);
    else
        ftruncate(m_cacheFileDescriptor, 0);
}

- (BOOL)cacheBytecodeWithError:(NSError **)error
{
    String errorString { };
    [self writeCache:errorString];
    if (!errorString.isNull()) {
        createError(errorString, error);
        return NO;
    }

    return YES;
}

- (NSURL *)sourceURL
{
    return m_sourceURL.get();
}

- (JSScriptType)type
{
    return m_type;
}

@end

@implementation JSScript(Internal)

- (instancetype)init
{
    self = [super init];
    if (!self)
        return nil;

    m_cacheFileDescriptor = -1;
    return self;
}

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

- (JSC::JSSourceCode*)jsSourceCode
{
    if (m_jsSourceCode)
        return m_jsSourceCode.get();

    return [self forceRecreateJSSourceCode];
}

- (BOOL)writeCache:(String&)error
{
    if (m_cachedBytecode.size()) {
        error = "Cache for JSScript is already non-empty. Can not override it."_s;
        return NO;
    }

    if (m_cacheFileDescriptor == -1) {
        if (!m_cachePath)
            error = "No cache was path provided during construction of this JSScript."_s;
        else
            error = "Could not lock the bytecode cache file. It's likely another VM or process is already using it."_s;
        return NO;
    }

    JSC::ParserError parserError;
    switch (m_type) {
    case kJSScriptTypeModule:
        m_cachedBytecode = JSC::generateModuleBytecode(m_virtualMachine.vm, [self jsSourceCode]->sourceCode(), parserError);
        break;
    case kJSScriptTypeProgram:
        m_cachedBytecode = JSC::generateProgramBytecode(m_virtualMachine.vm, [self jsSourceCode]->sourceCode(), parserError);
        break;
    }

    if (parserError.isValid()) {
        m_cachedBytecode = { };
        error = makeString("Unable to generate bytecode for this JSScript because of a parser error: ", parserError.message());
        return NO;
    }

    ssize_t bytesWritten = write(m_cacheFileDescriptor, m_cachedBytecode.data(), m_cachedBytecode.size());
    if (bytesWritten == -1) {
        error = makeString("Could not write cache file to disk: ", strerror(errno));
        return NO;
    }

    if (static_cast<size_t>(bytesWritten) != m_cachedBytecode.size()) {
        ftruncate(m_cacheFileDescriptor, 0);
        error = makeString("Could not write the full cache file to disk. Only wrote ", String::number(bytesWritten), " of the expected ", String::number(m_cachedBytecode.size()), " bytes.");
        return NO;
    }

    return YES;
}

- (void)setSourceURL:(NSURL *)url
{
    m_sourceURL = url;
}

- (JSC::JSSourceCode*)forceRecreateJSSourceCode
{
    JSC::VM& vm = m_virtualMachine.vm;
    JSC::JSLockHolder locker(vm);

    TextPosition startPosition { };

    String url = String { [[self sourceURL] absoluteString] };
    auto type = m_type == kJSScriptTypeModule ? JSC::SourceProviderSourceType::Module : JSC::SourceProviderSourceType::Program;
    Ref<JSScriptSourceProvider> sourceProvider = JSScriptSourceProvider::create(self, JSC::SourceOrigin(url), URL({ }, url), startPosition, type);
    JSC::SourceCode sourceCode(WTFMove(sourceProvider), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
    JSC::JSSourceCode* jsSourceCode = JSC::JSSourceCode::create(vm, WTFMove(sourceCode));
    m_jsSourceCode.set(vm, jsSourceCode);
    return jsSourceCode;
}

@end

#endif
