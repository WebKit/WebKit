/**
 * Copyright (C) 2019-2023 Apple Inc. All rights reserved.
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

#include "config.h"
#include "JSAPIGlobalObject.h"

#include "APICast.h"
#include "BytecodeCacheError.h"
#include "Completion.h"
#include "GlobalObjectMethodTable.h"
#include "JSModuleLoader.h"
#include "JSNativeStdFunction.h"
#include "JSPromise.h"
#include "JSSourceCode.h"
#include "JSString.h"
#include "JSCellInlines.h"
#include "LLIntThunks.h"
#include "ObjectConstructor.h"
#include "StructureCreateInlines.h"

#include <limits.h>
#include <span>
#include <sys/stat.h>
#include <sys/types.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/MappedFileData.h>
#include <wtf/Scope.h>
#include <wtf/URL.h>

#if OS(WINDOWS)
#include <direct.h>
#endif

#if !JSC_OBJC_API_ENABLED

template<typename Vector>
static inline String stringFromUTF(const Vector& utf8)
{
    return String::fromUTF8WithLatin1Fallback(utf8.span());
}

template<typename Vector>
static void convertShebangToJSComment(Vector& buffer)
{
    if (buffer.size() >= 2) {
        if (buffer[0] == '#' && buffer[1] == '!')
            buffer[0] = buffer[1] = '/';
    }
}

template<typename Vector>
static bool fetchModuleFromLocalFileSystem(const URL& fileURL, Vector& buffer)
{
    String fileName = fileURL.fileSystemPath();
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // Use long UNC to pass the long path name to the Windows APIs.
    // These also appear to turn off handling forward slashes as
    // directory separators as it disables all string parsing on names.
    fileName = makeStringByReplacingAll(fileName, '/', '\\');
    auto pathName = makeString("\\\\?\\"_s, fileName).wideCharacters();
    struct _stat status { };
    if (_wstat(pathName.span().data(), &status))
        return false;
    if ((status.st_mode & S_IFMT) != S_IFREG)
        return false;

    FILE* f = _wfopen(pathName.span().data(), L"rb");
#else
    auto pathName = fileName.utf8();
    struct stat status { };
    if (stat(pathName.data(), &status))
        return false;
    if ((status.st_mode & S_IFMT) != S_IFREG)
        return false;

    FILE* f = fopen(pathName.data(), "rb");
#endif
    if (!f) {
        fprintf(stderr, "Could not open module: %s\n", fileName.utf8().data());
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    if (result)
        convertShebangToJSComment(buffer);
    fclose(f);

    return result;
}

static UChar pathSeparator()
{
#if OS(WINDOWS)
    return '\\';
#else
    return '/';
#endif
}

static URL currentWorkingDirectory()
{
#if OS(WINDOWS)
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa364934.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/aa365247.aspx#maxpath
    // The _MAX_PATH in Windows is 260. If the path of the current working directory is longer than that, _getcwd truncates the result.
    // And other I/O functions taking a path name also truncate it. To avoid this situation,
    //
    // (1). When opening the file in Windows for modules, we always use the abosolute path and add "\\?\" prefix to the path name.
    // (2). When retrieving the current working directory, use GetCurrentDirectory instead of _getcwd.
    //
    // In the path utility functions inside the JSC shell, we does not handle the UNC and UNCW including the network host name.
    DWORD bufferLength = ::GetCurrentDirectoryW(0, nullptr);
    if (!bufferLength)
        return { };
    // In Windows, wchar_t is the UTF-16LE.
    // https://msdn.microsoft.com/en-us/library/dd374081.aspx
    // https://msdn.microsoft.com/en-us/library/windows/desktop/ff381407.aspx
    Vector<wchar_t> buffer(bufferLength);
    DWORD lengthNotIncludingNull = ::GetCurrentDirectoryW(bufferLength, buffer.data());
    String directoryString(buffer.data(), lengthNotIncludingNull);
    // We don't support network path like \\host\share\<path name>.
    if (directoryString.startsWith("\\\\"_s))
        return { };

#else
    Vector<char> buffer(PATH_MAX);
    if (!getcwd(buffer.data(), PATH_MAX))
        return { };
    String directoryString = String::fromUTF8(buffer.data());
#endif
    if (directoryString.isEmpty())
        return { };

    // Add a trailing slash if needed so the URL resolves to a directory and not a file.
    if (directoryString[directoryString.length() - 1] != pathSeparator())
        directoryString = makeString(directoryString, pathSeparator());

    return URL::fileURLWithFileSystemPath(directoryString);
}

// FIXME: We may wish to support module specifiers beginning with a (back)slash on Windows. We could either:
// - align with V8 and SM:  treat '/foo' as './foo'
// - align with PowerShell: treat '/foo' as 'C:/foo'
static bool isAbsolutePath(StringView path)
{
#if OS(WINDOWS)
    // Just look for local drives like C:\.
    return path.length() > 2 && isASCIIAlpha(path[0]) && path[1] == ':' && (path[2] == '\\' || path[2] == '/');
#else
    return path.startsWith('/');
#endif
}

static bool isDottedRelativePath(StringView path)
{
#if OS(WINDOWS)
    auto length = path.length();
    if (length < 2 || path[0] != '.')
        return false;

    if (path[1] == '/' || path[1] == '\\')
        return true;

    return length > 2 && path[1] == '.' && (path[2] == '/' || path[2] == '\\');
#else
    return path.startsWith("./"_s) || path.startsWith("../"_s);
#endif
}

static bool isFileModule(StringView path)
{
    return path.startsWith("file://"_s) || isAbsolutePath(path) || isDottedRelativePath(path);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

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
    buffer.grow(bufferCapacity + initialSize);
    auto span = buffer.mutableSpan().subspan(initialSize);
    size_t readSize = fread(span.data(), 1, span.size(), file);
    return readSize == buffer.size() - initialSize;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

class APISourceProvider final : public JSC::StringSourceProvider {
public:
    static Ref<APISourceProvider> create(const String& source, const JSC::SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, JSC::SourceProviderSourceType sourceType)
    {
        return adoptRef(*new APISourceProvider(source, sourceOrigin, WTF::move(sourceURL), startPosition, sourceType));
    }

    ~APISourceProvider() final
    {
        commitCachedBytecode();
    }

    RefPtr<JSC::CachedBytecode> cachedBytecode() const final
    {
        if (!m_cachedBytecode)
            loadBytecode();
        return m_cachedBytecode.copyRef();
    }

    void updateCache(const JSC::UnlinkedFunctionExecutable* executable, const JSC::SourceCode&, JSC::CodeSpecializationKind kind, const JSC::UnlinkedFunctionCodeBlock* codeBlock) const final
    {
        if (!cacheEnabled() || !m_cachedBytecode)
            return;
        JSC::BytecodeCacheError error;
        RefPtr<JSC::CachedBytecode> cachedBytecode = encodeFunctionCodeBlock(executable->vm(), codeBlock, error);
        if (cachedBytecode && !error.isValid())
            m_cachedBytecode->addFunctionUpdate(executable, kind, *cachedBytecode);
    }

    void cacheBytecode(const JSC::BytecodeCacheGenerator& generator) const final
    {
        if (!cacheEnabled())
            return;
        if (!m_cachedBytecode)
            m_cachedBytecode = JSC::CachedBytecode::create();
        auto update = generator();
        if (update)
            m_cachedBytecode->addGlobalUpdate(*update);
    }

    void commitCachedBytecode() const final
    {
        if (!cacheEnabled() || !m_cachedBytecode || !m_cachedBytecode->hasUpdates())
            return;

        auto clearBytecode = makeScopeExit([&] {
            m_cachedBytecode = nullptr;
        });

        String filename = cachePath();
        auto handle = FileSystem::openFile(filename, FileSystem::FileOpenMode::ReadWrite, FileSystem::FileAccessPermission::All, { FileSystem::FileLockMode::Exclusive, FileSystem::FileLockMode::Nonblocking });
        if (!handle)
            return;

        auto fileSize = handle.size();
        if (!fileSize)
            return;

        size_t cacheFileSize;
        if (!WTF::convertSafely(*fileSize, cacheFileSize) || cacheFileSize != m_cachedBytecode->size()) {
            // The bytecode cache has already been updated
            return;
        }

        if (!handle.truncate(m_cachedBytecode->sizeForUpdate()))
            return;

        m_cachedBytecode->commitUpdates([&] (off_t offset, std::span<const uint8_t> data) {
            auto result = handle.seek(offset, FileSystem::FileSeekOrigin::Beginning);
            ASSERT_UNUSED(result, !!result);
            auto bytesWritten = handle.write(data);
            ASSERT_UNUSED(bytesWritten, bytesWritten == data.size());
        });
    }

private:
    String cachePath() const
    {
        if (!cacheEnabled())
            return { };
        const char* cachePath = JSC::Options::diskCachePath();
        String filename = FileSystem::encodeForFileName(FileSystem::lastComponentOfPathIgnoringTrailingSlash(sourceOrigin().url().fileSystemPath()));
        return FileSystem::pathByAppendingComponent(StringView::fromLatin1(cachePath), makeString(source().hash(), '-', filename, ".bytecode-cache"_s));
    }

    void loadBytecode() const
    {
        if (!cacheEnabled())
            return;

        String filename = cachePath();
        if (filename.isNull())
            return;

        auto handle = FileSystem::openFile(filename, FileSystem::FileOpenMode::Read, FileSystem::FileAccessPermission::All, { FileSystem::FileLockMode::Shared, FileSystem::FileLockMode::Nonblocking });
        if (!handle)
            return;

        auto mappedFileData = handle.map(FileSystem::MappedFileMode::Private);
        if (!mappedFileData)
            return;

        m_cachedBytecode = JSC::CachedBytecode::create(WTF::move(*mappedFileData));
    }

    APISourceProvider(const String& source, const JSC::SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, JSC::SourceProviderSourceType sourceType)
        : JSC::StringSourceProvider(source, sourceOrigin, JSC::SourceTaintedOrigin::Untainted, WTF::move(sourceURL), startPosition, sourceType)
        // Workers started via $.agent.start are not shut down in a synchronous manner, and it
        // is possible the main thread terminates the process while a worker is writing its
        // bytecode cache, which results in intermittent test failures. As $.agent.start is only
        // a rarely used testing facility, we simply do not cache bytecode on these threads.
        , m_cacheEnabled(!!JSC::Options::diskCachePath())
    {
    }

    bool cacheEnabled() const { return m_cacheEnabled; }

    mutable RefPtr<JSC::CachedBytecode> m_cachedBytecode;
    const bool m_cacheEnabled;
};

static inline JSC::SourceCode jscSource(const String& source, const JSC::SourceOrigin& sourceOrigin, String sourceURL = String(), const TextPosition& startPosition = TextPosition(), JSC::SourceProviderSourceType sourceType = JSC::SourceProviderSourceType::Program)
{
    return JSC::SourceCode(APISourceProvider::create(source, sourceOrigin, WTF::move(sourceURL), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
}

template<typename Vector>
static inline JSC::SourceCode jscSource(const Vector& utf8, const JSC::SourceOrigin& sourceOrigin, const String& filename)
{
    // FIXME: This should use an absolute file URL https://bugs.webkit.org/show_bug.cgi?id=193077
    String str = stringFromUTF(utf8);
    return jscSource(str, sourceOrigin, filename);
}

#endif // !JSC_OBJC_API_ENABLED

namespace JSC {

const ClassInfo JSAPIGlobalObject::s_info = { "GlobalObject"_s, &Base::s_info, nullptr, nullptr, CREATE_METHOD_TABLE(JSAPIGlobalObject) };

#if !JSC_OBJC_API_ENABLED

const GlobalObjectMethodTable* JSAPIGlobalObject::globalObjectMethodTable()
{
    static constexpr GlobalObjectMethodTable table {
        &supportsRichSourceInfo,
        &shouldInterruptScript,
        &javaScriptRuntimeFlags,
        &shouldInterruptScriptBeforeTimeout,
        &moduleLoaderImportModule,
        &moduleLoaderResolve,
        &moduleLoaderFetch,
        &moduleLoaderCreateImportMetaProperties,
        &moduleLoaderEvaluate,
        &promiseRejectionTracker,
        &reportUncaughtExceptionAtEventLoop,
        &currentScriptExecutionOwner,
        &scriptExecutionStatus,
        &reportViolationForUnsafeEval,
        nullptr, // defaultLanguage
        nullptr, // compileStreaming
        nullptr, // instantiateStreaming
        nullptr, // deriveShadowRealmGlobalObject
        &codeForEval,
        &canCompileStrings,
        &trustedScriptStructure,
    };
    return &table;
}

void JSAPIGlobalObject::reportUncaughtExceptionAtEventLoop(JSGlobalObject* globalObject, Exception* exception)
{
    Base::reportUncaughtExceptionAtEventLoop(globalObject, exception);
}

#endif // !JSC_OBJC_API_ENABLED

JSAPIGlobalObject::JSAPIGlobalObject(VM& vm, Structure* structure)
    : Base(vm, structure, globalObjectMethodTable())
{
}

JSAPIGlobalObject* JSAPIGlobalObject::create(VM& vm, Structure* structure)
{
    auto* object = new (NotNull, allocateCell<JSAPIGlobalObject>(vm)) JSAPIGlobalObject(vm, structure);
    object->finishCreation(vm);
    return object;
}

Structure* JSAPIGlobalObject::createStructure(VM& vm, JSValue prototype)
{
    auto* result = Structure::create(vm, nullptr, prototype, TypeInfo(GlobalObjectType, StructureFlags), info());
    result->setTransitionWatchpointIsLikelyToBeFired(true);
    return result;
}

#if !JSC_OBJC_API_ENABLED

JSPromise* JSAPIGlobalObject::moduleLoaderImportModule(JSGlobalObject* globalObject, JSModuleLoader*, JSString* moduleNameValue, RefPtr<ScriptFetchParameters> fetchParams, const SourceOrigin& sourceOrigin)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto rejectWithCaughtException = [&]() -> JSPromise* {
        auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
        return promise->rejectWithCaughtException(globalObject, scope);
    };

    auto& referrer = sourceOrigin.url();
    auto specifier = moduleNameValue->value(globalObject);
    if (scope.exception()) [[unlikely]]
        return rejectWithCaughtException();

    if (!referrer.protocolIsFile()) [[unlikely]] {
        auto* promise = JSPromise::create(vm, globalObject->promiseStructure());
        promise->reject(vm, globalObject, createError(globalObject, makeString("Could not resolve the referrer's path '"_s, referrer.string(), "', while trying to resolve module '"_s, specifier.data, "'."_s)));
        return promise;
    }

    auto* result = JSC::importModule(globalObject, Identifier::fromString(vm, specifier), Identifier::fromString(vm, referrer.string()), WTF::move(fetchParams), nullptr);
    if (scope.exception()) [[unlikely]]
        return rejectWithCaughtException();

    return result;
}

Identifier JSAPIGlobalObject::moduleLoaderResolve(JSGlobalObject* globalObject, JSModuleLoader*, JSValue keyValue, JSValue referrerValue, RefPtr<ScriptFetcher>, bool)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    scope.releaseAssertNoException();
    const Identifier key = keyValue.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (key.isSymbol())
        return key;

    auto* apiGlobalObject = jsCast<JSAPIGlobalObject*>(globalObject);
    if (apiGlobalObject->hasAPIModuleLoaderResolve()) {
        String specifier = key.impl();
        if (apiGlobalObject->api_moduleLoader.disableBuiltinFileSystemLoader || !isFileModule(specifier)) {
            JSContextRef contextRef = toRef(globalObject);
            JSStringRef resolved = apiGlobalObject->api_moduleLoader.moduleLoaderResolve(contextRef, toRef(globalObject, keyValue), toRef(globalObject, referrerValue), toRef(globalObject, jsUndefined()));
            if (!resolved) {
                throwTypeError(globalObject, scope, "Module resolver returned null"_s);
                return { };
            }

            Identifier resolvedKey = Identifier::fromString(vm, resolved->string());
            resolved->deref();
            return resolvedKey;
        }
    }

    auto resolvePath = [&] (const URL& directoryURL) -> Identifier {
        String specifier = key.impl();
        auto filePrefix = "file://"_s;
        if (specifier.startsWith(filePrefix))
            specifier = specifier.substringSharingImpl(filePrefix.length());

        bool specifierIsAbsolute = isAbsolutePath(specifier);
        if (!specifierIsAbsolute && !isDottedRelativePath(specifier)) {
            throwTypeError(globalObject, scope, makeString("Module specifier, '"_s, specifier, "' is not absolute and does not start with \"./\" or \"../\". Referenced from: "_s, directoryURL.fileSystemPath()));
            return { };
        }

        if (!directoryURL.protocolIsFile()) {
            throwException(globalObject, scope, createError(globalObject, makeString("Could not resolve the referrer's path: "_s, directoryURL.string())));
            return { };
        }

        auto resolvedURL = specifierIsAbsolute ? URL::fileURLWithFileSystemPath(specifier) : URL(directoryURL, specifier);
        if (!resolvedURL.isValid()) {
            throwException(globalObject, scope, createError(globalObject, makeString("Resolved module url is not valid: "_s, resolvedURL.string())));
            return { };
        }
        ASSERT(resolvedURL.protocolIsFile());

        return Identifier::fromString(vm, resolvedURL.string());
    };

    if (referrerValue.isUndefined())
        return resolvePath(currentWorkingDirectory());

    const Identifier referrer = referrerValue.toPropertyKey(globalObject);
    RETURN_IF_EXCEPTION(scope, { });

    if (referrer.isSymbol())
        return resolvePath(currentWorkingDirectory());

    // If the referrer exists, we assume that the referrer is the correct file url.
    URL url = URL({ }, referrer.impl());
    ASSERT(url.protocolIsFile());
    return resolvePath(url);
}

JSValue JSAPIGlobalObject::moduleLoaderEvaluate(JSGlobalObject* globalObject, JSModuleLoader* moduleLoader, JSValue key, JSValue moduleRecordValue, RefPtr<ScriptFetcher> scriptFetcher, JSValue sentValue, JSValue resumeMode)
{
    return moduleLoader->evaluateNonVirtual(globalObject, key, moduleRecordValue, WTF::move(scriptFetcher), sentValue, resumeMode);
}

JSPromise* JSAPIGlobalObject::moduleLoaderFetch(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, RefPtr<ScriptFetchParameters> attributes, RefPtr<ScriptFetcher>)
{
    VM& vm = globalObject->vm();
    JSPromise* promise = JSPromise::create(vm, globalObject->promiseStructure());

    auto scope = DECLARE_THROW_SCOPE(vm);

    auto rejectWithError = [&](JSValue error) {
        promise->reject(vm, globalObject, error);
        return promise;
    };

    String moduleKey = key.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

    URL moduleURL({ }, moduleKey);
    auto* apiGlobalObject = jsCast<JSAPIGlobalObject*>(globalObject);

    if (apiGlobalObject->isSyntheticModuleKey(moduleKey) && apiGlobalObject->hasAPIModuleLoaderEvaluate()) {
        auto sourceCode = JSSourceCode::create(vm, jscSource(String(), SourceOrigin { moduleURL }, String { moduleKey }, TextPosition(), SourceProviderSourceType::Module));
        scope.release();
        promise->resolve(globalObject, vm, sourceCode);
        return promise;
    }

    if (apiGlobalObject->hasAPIModuleLoaderFetch() && (apiGlobalObject->api_moduleLoader.disableBuiltinFileSystemLoader || !moduleURL.protocolIsFile())) {
        JSContextRef contextRef = toRef(globalObject);
        JSStringRef sourceRef = apiGlobalObject->api_moduleLoader.moduleLoaderFetch(contextRef, toRef(globalObject, key), toRef(globalObject, jsUndefined()), toRef(globalObject, jsUndefined()));
        RETURN_IF_EXCEPTION(scope, promise->rejectWithCaughtException(globalObject, scope));

        if (!sourceRef)
            RELEASE_AND_RETURN(scope, rejectWithError(createError(globalObject, "Module fetcher returned null"_s)));

        String sourceString = sourceRef->string();
        sourceRef->deref();

        if ((attributes && attributes->type() == ScriptFetchParameters::Type::JSON) || moduleKey.endsWith(".json"_s)) {
            auto source = SourceCode(StringSourceProvider::create(sourceString, SourceOrigin { moduleURL }, String { moduleKey }, SourceTaintedOrigin::Untainted, TextPosition(), SourceProviderSourceType::JSON));
            auto sourceCode = JSSourceCode::create(vm, WTF::move(source));
            scope.release();
            promise->resolve(globalObject, vm, sourceCode);
            return promise;
        }

        auto sourceCode = JSSourceCode::create(vm, jscSource(sourceString, SourceOrigin { moduleURL }, String { moduleKey }, TextPosition(), SourceProviderSourceType::Module));
        scope.release();
        promise->resolve(globalObject, vm, sourceCode);
        return promise;
    }

    ASSERT(moduleURL.protocolIsFile());
    // Strip the URI from our key so Errors print canonical system paths.
    moduleKey = moduleURL.fileSystemPath();

    Vector<uint8_t> buffer;
    if (!fetchModuleFromLocalFileSystem(moduleURL, buffer))
        RELEASE_AND_RETURN(scope, rejectWithError(createError(globalObject, makeString("Could not open file '"_s, moduleKey, "'."_s))));

    if (attributes && attributes->type() == ScriptFetchParameters::Type::JSON) {
        auto source = SourceCode(StringSourceProvider::create(stringFromUTF(buffer), SourceOrigin { moduleURL }, WTF::move(moduleKey), SourceTaintedOrigin::Untainted, TextPosition(), SourceProviderSourceType::JSON));
        auto sourceCode = JSSourceCode::create(vm, WTF::move(source));
        scope.release();
        promise->resolve(globalObject, vm, sourceCode);
        return promise;
    }

    auto sourceCode = JSSourceCode::create(vm, jscSource(stringFromUTF(buffer), SourceOrigin { moduleURL }, WTF::move(moduleKey), TextPosition(), SourceProviderSourceType::Module));
    scope.release();
    promise->resolve(globalObject, vm, sourceCode);
    return promise;
}

JSObject* JSAPIGlobalObject::moduleLoaderCreateImportMetaProperties(JSGlobalObject* globalObject, JSModuleLoader*, JSValue key, JSModuleRecord*, RefPtr<ScriptFetcher>)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    auto* apiGlobalObject = jsCast<JSAPIGlobalObject*>(globalObject);
    if (apiGlobalObject->hasAPIModuleLoaderCreateImportMetaProperties()) {
        JSContextRef contextRef = toRef(globalObject);
        JSObjectRef object = apiGlobalObject->api_moduleLoader.moduleLoaderCreateImportMetaProperties(contextRef, toRef(globalObject, key), toRef(globalObject, jsUndefined()));
        if (object)
            return toJS(object);
    }

    JSObject* metaProperties = constructEmptyObject(vm, globalObject->nullPrototypeObjectStructure());
    RETURN_IF_EXCEPTION(scope, nullptr);

    String modulePath = key.toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, nullptr);

    String dirname;
    String filename = modulePath;
    URL moduleURL({ }, modulePath);
    if (moduleURL.protocolIsFile()) {
        modulePath = moduleURL.fileSystemPath();
        if (auto separatorIndex = modulePath.reverseFind(pathSeparator()); separatorIndex != notFound) {
            dirname = modulePath.substring(0, separatorIndex);
            filename = modulePath.substring(separatorIndex + 1);
        } else
            filename = modulePath;
    }

    metaProperties->putDirect(vm, Identifier::fromString(vm, "url"_s), key);
    RETURN_IF_EXCEPTION(scope, nullptr);
    metaProperties->putDirect(vm, Identifier::fromString(vm, "dir"_s), jsString(vm, dirname));
    RETURN_IF_EXCEPTION(scope, nullptr);
    metaProperties->putDirect(vm, Identifier::fromString(vm, "filename"_s), jsString(vm, filename));
    RETURN_IF_EXCEPTION(scope, nullptr);

    return metaProperties;
}

#endif // !JSC_OBJC_API_ENABLED

JSValue JSAPIGlobalObject::loadAndEvaluateJSScriptModule(const JSLockHolder&, JSScript *script)
{
    UNUSED_PARAM(script);
    // FIXME: Implement JSScript module evaluation for the C API.
    return jsUndefined();
}

}
