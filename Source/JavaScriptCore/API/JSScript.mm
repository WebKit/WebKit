/*
 * Copyright (C) 2019-2022 Apple Inc. All rights reserved.
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
#import "JSBase.h"
#import "JSScriptInternal.h"

#import "APICast.h"
#import "BytecodeCacheError.h"
#import "CachedTypes.h"
#import "CodeCache.h"
#import "Completion.h"
#import "Identifier.h"
#import "IntegrityInlines.h"
#import "JSContextInternal.h"
#import "JSScriptSourceProvider.h"
#import "JSSourceCode.h"
#import "JSValuePrivate.h"
#import "JSVirtualMachineInternal.h"
#import "Symbol.h"
#import <sys/stat.h>
#import <wtf/FileHandle.h>
#import <wtf/FileSystem.h>
#import <wtf/SHA1.h>
#import <wtf/SafeStrerror.h>
#import <wtf/Scope.h>
#import <wtf/StdLibExtras.h>
#import <wtf/WeakObjCPtr.h>
#import <wtf/spi/darwin/DataVaultSPI.h>
#import <wtf/text/MakeString.h>

#if JSC_OBJC_API_ENABLED

@implementation JSScript {
    WeakObjCPtr<JSVirtualMachine> m_virtualMachine;
    JSScriptType m_type;
    FileSystem::MappedFileData m_mappedSource;
    String m_source;
    RetainPtr<NSURL> m_sourceURL;
    RetainPtr<NSURL> m_cachePath;
    RefPtr<JSC::CachedBytecode> m_cachedBytecode;
}

static JSScript *createError(NSString *message, NSError** error)
{
    if (error)
        *error = [NSError errorWithDomain:@"JSScriptErrorDomain" code:1 userInfo:@{ @"message": message }];
    return nil;
}

static bool validateBytecodeCachePath(NSURL* cachePath, NSError** error)
{
    if (!cachePath)
        return true;

    URL cachePathURL([cachePath absoluteURL]);
    if (!cachePathURL.protocolIsFile()) {
        createError([NSString stringWithFormat:@"Cache path `%@` is not a local file", cachePathURL.createNSURL().get()], error);
        return false;
    }

    String systemPath = cachePathURL.fileSystemPath();

    if (auto fileType = FileSystem::fileType(systemPath)) {
        if (*fileType != FileSystem::FileType::Regular) {
            createError([NSString stringWithFormat:@"Cache path `%@` already exists and is not a file", systemPath.createNSString().get()], error);
            return false;
        }
    }

    String directory = FileSystem::parentPath(systemPath);
    if (directory.isNull()) {
        createError([NSString stringWithFormat:@"Cache path `%@` does not contain in a valid directory", systemPath.createNSString().get()], error);
        return false;
    }

    if (FileSystem::fileType(directory) != FileSystem::FileType::Directory) {
        createError([NSString stringWithFormat:@"Cache directory `%@` is not a directory or does not exist", directory.createNSString().get()], error);
        return false;
    }

#if USE(APPLE_INTERNAL_SDK)
    if (rootless_check_datavault_flag(FileSystem::fileSystemRepresentation(directory).data(), nullptr)) {
        createError([NSString stringWithFormat:@"Cache directory `%@` is not a data vault", directory.createNSString().get()], error);
        return false;
    }
#endif

    return true;
}

+ (instancetype)scriptOfType:(JSScriptType)type withSource:(NSString *)source andSourceURL:(NSURL *)sourceURL andBytecodeCache:(NSURL *)cachePath inVirtualMachine:(JSVirtualMachine *)vm error:(out NSError **)error
{
    if (!validateBytecodeCachePath(cachePath, error))
        return nil;

    auto result = adoptNS([[JSScript alloc] init]);
    result->m_virtualMachine = vm;
    result->m_type = type;
    result->m_source = source;
    result->m_sourceURL = sourceURL;
    result->m_cachePath = cachePath;
    [result readCache];
    return result.autorelease();
}

+ (instancetype)scriptOfType:(JSScriptType)type memoryMappedFromASCIIFile:(NSURL *)filePath withSourceURL:(NSURL *)sourceURL andBytecodeCache:(NSURL *)cachePath inVirtualMachine:(JSVirtualMachine *)vm error:(out NSError **)error
{
    if (!validateBytecodeCachePath(cachePath, error))
        return nil;

    URL filePathURL([filePath absoluteURL]);
    if (!filePathURL.protocolIsFile())
        return createError([NSString stringWithFormat:@"File path %@ is not a local file", filePathURL.createNSURL().get()], error);

    String systemPath = filePathURL.fileSystemPath();
    auto fileData = FileSystem::mapFile(systemPath, FileSystem::MappedFileMode::Shared);
    if (!fileData)
        return createError([NSString stringWithFormat:@"File at path %@ could not be mapped.", systemPath.createNSString().get()], error);

    if (!charactersAreAllASCII(fileData->span()))
        return createError([NSString stringWithFormat:@"Not all characters in file at %@ are ASCII.", systemPath.createNSString().get()], error);

    auto result = adoptNS([[JSScript alloc] init]);
    result->m_virtualMachine = vm;
    result->m_type = type;
    result->m_source = StringImpl::createWithoutCopying(byteCast<Latin1Character>(fileData->span()));
    result->m_mappedSource = WTFMove(*fileData);
    result->m_sourceURL = sourceURL;
    result->m_cachePath = cachePath;
    [result readCache];
    return result.autorelease();
}

- (void)readCache
{
    if (!m_cachePath)
        return;

    String cacheFilename = [m_cachePath path];

    auto handle = FileSystem::openFile(cacheFilename, FileSystem::FileOpenMode::Read, FileSystem::FileAccessPermission::All, { FileSystem::FileLockMode::Exclusive, FileSystem::FileLockMode::Nonblocking });
    if (!handle)
        return;

    auto mappedFile = handle.map(FileSystem::MappedFileMode::Private);
    if (!mappedFile)
        return;

    auto fileData = mappedFile->span();

    // Ensure we at least have a SHA1::Digest to read.
    if (fileData.size() < sizeof(SHA1::Digest)) {
        FileSystem::deleteFile(cacheFilename);
        return;
    }

    unsigned fileDataSize = fileData.size() - sizeof(SHA1::Digest);

    SHA1::Digest computedHash;
    SHA1 sha1;
    sha1.addBytes(fileData.first(fileDataSize));
    sha1.computeHash(computedHash);

    SHA1::Digest fileHash;
    auto hashSpan = fileData.subspan(fileDataSize, sizeof(SHA1::Digest));
    memcpySpan(std::span { fileHash }, hashSpan);

    if (computedHash != fileHash) {
        FileSystem::deleteFile(cacheFilename);
        return;
    }

    Ref cachedBytecode = JSC::CachedBytecode::create(WTFMove(*mappedFile));

    JSC::VM& vm = *toJS([m_virtualMachine JSContextGroupRef]);
    JSC::SourceCode sourceCode = [self sourceCode];
    JSC::SourceCodeKey key = m_type == kJSScriptTypeProgram ? sourceCodeKeyForSerializedProgram(vm, sourceCode) : sourceCodeKeyForSerializedModule(vm, sourceCode);
    if (isCachedBytecodeStillValid(vm, cachedBytecode.copyRef(), key, m_type == kJSScriptTypeProgram ? JSC::SourceCodeType::ProgramType : JSC::SourceCodeType::ModuleType))
        m_cachedBytecode = WTFMove(cachedBytecode);
    else
        handle.truncate(0);
}

- (BOOL)cacheBytecodeWithError:(NSError **)error
{
    String errorString { };
    [self writeCache:errorString];
    if (!errorString.isNull()) {
        createError(errorString.createNSString().get(), error);
        return NO;
    }

    return YES;
}

- (BOOL)isUsingBytecodeCache
{
    return !!m_cachedBytecode->size();
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

    self->m_cachedBytecode = JSC::CachedBytecode::create();

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

- (RefPtr<JSC::CachedBytecode>)cachedBytecode
{
    return m_cachedBytecode;
}

- (JSC::SourceCode)sourceCode
{
    JSC::VM& vm = *toJS([m_virtualMachine JSContextGroupRef]);
    JSC::JSLockHolder locker(vm);

    TextPosition startPosition { };
    String filename = String { [[self sourceURL] absoluteString] };
    URL url = URL({ }, filename);
    auto type = m_type == kJSScriptTypeModule ? JSC::SourceProviderSourceType::Module : JSC::SourceProviderSourceType::Program;
    JSC::SourceOrigin origin(url);
    Ref<JSScriptSourceProvider> sourceProvider = JSScriptSourceProvider::create(self, origin, WTFMove(filename), String(), JSC::SourceTaintedOrigin::Untainted, startPosition, type);
    JSC::SourceCode sourceCode(WTFMove(sourceProvider), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
    return sourceCode;
}

- (JSC::JSSourceCode*)jsSourceCode
{
    JSC::VM& vm = *toJS([m_virtualMachine JSContextGroupRef]);
    JSC::JSLockHolder locker(vm);
    JSC::JSSourceCode* jsSourceCode = JSC::JSSourceCode::create(vm, [self sourceCode]);
    return jsSourceCode;
}

- (BOOL)writeCache:(String&)error
{
    if (self.isUsingBytecodeCache) {
        error = "Cache for JSScript is already non-empty. Can not override it."_s;
        return NO;
    }

    if (!m_cachePath) {
        error = "No cache path was provided during construction of this JSScript."_s;
        return NO;
    }

    // We want to do the write as a transaction (i.e. we guarantee that it's all
    // or nothing). So, we'll write to a temp file first, and rename the temp
    // file to the cache file only after we've finished writing the whole thing.

    NSString *cachePathString = [m_cachePath path];
    const char* cacheFileName = cachePathString.UTF8String;
    const char* tempFileName = [cachePathString stringByAppendingString:@".tmp"].UTF8String;
    auto fileHandle = FileSystem::FileHandle::adopt(open(cacheFileName, O_CREAT | O_WRONLY | O_EXLOCK | O_NONBLOCK, 0600));
    if (!fileHandle) {
        error = makeString("Could not open or lock the bytecode cache file. It's likely another VM or process is already using it. Error: "_s, safeStrerror(errno).span());
        return NO;
    }

    auto tempFileHandle = FileSystem::FileHandle::adopt(open(tempFileName, O_CREAT | O_RDWR | O_EXLOCK | O_NONBLOCK, 0600));
    if (!tempFileHandle) {
        error = makeString("Could not open or lock the bytecode cache temp file. Error: "_s, safeStrerror(errno).span());
        return NO;
    }

    JSC::BytecodeCacheError cacheError;
    JSC::SourceCode sourceCode = [self sourceCode];
    JSC::VM& vm = *toJS([m_virtualMachine JSContextGroupRef]);
    switch (m_type) {
    case kJSScriptTypeModule:
        m_cachedBytecode = JSC::generateModuleBytecode(vm, sourceCode, tempFileHandle, cacheError);
        break;
    case kJSScriptTypeProgram:
        m_cachedBytecode = JSC::generateProgramBytecode(vm, sourceCode, tempFileHandle, cacheError);
        break;
    }

    if (cacheError.isValid()) {
        m_cachedBytecode = JSC::CachedBytecode::create();
        fileHandle.truncate(0);
        error = makeString("Unable to generate bytecode for this JSScript because: "_s, cacheError.message());
        return NO;
    }

    SHA1::Digest computedHash;
    SHA1 sha1;
    sha1.addBytes(m_cachedBytecode->span());
    sha1.computeHash(computedHash);
    tempFileHandle.write(computedHash);

    tempFileHandle.flush();
    rename(tempFileName, cacheFileName);
    return YES;
}

@end

#endif
