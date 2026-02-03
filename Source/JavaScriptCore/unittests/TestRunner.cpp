/*
 * Copyright (C) 2025 Apple Inc. All rights reserved.
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

#if !OS(WINDOWS)
// Test registration relies on linker sections, but in Windows linker sections
// are too different, in particular the way of discovering their start and
// end pointers. For now we only test if we are not on Windows.

#include "BytecodeCacheError.h"
#include "Completion.h"
#include "JSCJSValueInlines.h"
#include "SourceCode.h"
#include "Test.h"
#include "UnlinkedFunctionExecutable.h"

#include <filesystem>
#include <sys/stat.h>
#include <wtf/FileHandle.h>
#include <wtf/FileSystem.h>
#include <wtf/Scope.h>

namespace JSC {

/*
        Support infrastructure and the implementation of 'load()' copied from 'jsc.cpp'.
*/

JSC_DECLARE_HOST_FUNCTION(functionLoad);
JSC_DECLARE_HOST_FUNCTION(functionProbe);

extern TestProbe currentTestProbe;

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

static void dumpException(JSGlobalObject* globalObject, JSValue exception)
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_CATCH_SCOPE(vm);

#define CHECK_EXCEPTION() do { \
        if (scope.exception()) { \
            scope.clearException(); \
            return; \
        } \
    } while (false)

    auto exceptionString = exception.toWTFString(globalObject);
    CHECK_EXCEPTION();
    Expected<CString, UTF8ConversionError> expectedCString = exceptionString.tryGetUTF8();
    if (expectedCString)
        SAFE_PRINTF("Exception: %s\n", expectedCString.value());
    else
        SAFE_PRINTF("Exception: <out of memory while extracting exception string>\n");

    Identifier nameID = Identifier::fromString(vm, "name"_s);
    CHECK_EXCEPTION();
    Identifier fileNameID = Identifier::fromString(vm, "sourceURL"_s);
    CHECK_EXCEPTION();
    Identifier lineNumberID = Identifier::fromString(vm, "line"_s);
    CHECK_EXCEPTION();
    Identifier stackID = Identifier::fromString(vm, "stack"_s);
    CHECK_EXCEPTION();

    JSValue nameValue = exception.get(globalObject, nameID);
    CHECK_EXCEPTION();
    JSValue fileNameValue = exception.get(globalObject, fileNameID);
    CHECK_EXCEPTION();
    JSValue lineNumberValue = exception.get(globalObject, lineNumberID);
    CHECK_EXCEPTION();
    JSValue stackValue = exception.get(globalObject, stackID);
    CHECK_EXCEPTION();

    auto nameString = nameValue.toWTFString(globalObject);
    CHECK_EXCEPTION();

    if (nameString == "SyntaxError"_s && (!fileNameValue.isUndefinedOrNull() || !lineNumberValue.isUndefinedOrNull())) {
        auto fileNameString = fileNameValue.toWTFString(globalObject);
        CHECK_EXCEPTION();
        auto lineNumberString = lineNumberValue.toWTFString(globalObject);
        CHECK_EXCEPTION();
        SAFE_PRINTF("at %s:%s\n", fileNameString.utf8(), lineNumberString.utf8());
    }

    if (!stackValue.isUndefinedOrNull()) {
        auto stackString = stackValue.toWTFString(globalObject);
        CHECK_EXCEPTION();
        if (stackString.length()) {
            auto expectedUtf8 = stackString.tryGetUTF8();
            if (expectedUtf8)
                SAFE_PRINTF("%s\n", expectedUtf8.value());
        }
    }

    fflush(stdout);
#undef CHECK_EXCEPTION
}

static char16_t pathSeparator()
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
    DWORD lengthNotIncludingNull = ::GetCurrentDirectoryW(bufferLength, buffer.mutableSpan().data());
    String directoryString(buffer.span().data(), lengthNotIncludingNull);
    // We don't support network path like \\host\share\<path name>.
    if (directoryString.startsWith("\\\\"_s))
        return { };

#else
    Vector<char> buffer(PATH_MAX);
    if (!getcwd(buffer.mutableSpan().data(), PATH_MAX))
        return { };
    String directoryString = String::fromUTF8(buffer.span().data());
#endif
    if (directoryString.isEmpty())
        return { };

    // Add a trailing slash if needed so the URL resolves to a directory and not a file.
    if (directoryString[directoryString.length() - 1] != pathSeparator())
        directoryString = makeString(directoryString, pathSeparator());

    return URL::fileURLWithFileSystemPath(directoryString);
}

// FIXME: We may wish to support module specifiers beginning with a (back)slash on Windows. We could either:
// - align with V8 and SM: treat '/foo' as './foo'
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

static URL absoluteFileURL(const String& fileName)
{
    if (isAbsolutePath(fileName))
        return URL::fileURLWithFileSystemPath(fileName);

    auto directoryName = currentWorkingDirectory();
    if (!directoryName.isValid())
        return URL::fileURLWithFileSystemPath(fileName);

    return URL(directoryName, fileName);
}

static URL computeFilePath(VM& vm, JSGlobalObject* globalObject, CallFrame* callFrame)
{
    auto scope = DECLARE_THROW_SCOPE(vm);

    bool callerRelative = callFrame->argument(1).getString(globalObject) == "caller relative"_s;
    RETURN_IF_EXCEPTION(scope, URL());

    String fileName = callFrame->argument(0).toWTFString(globalObject);
    RETURN_IF_EXCEPTION(scope, URL());

    URL path;
    if (callerRelative) {
        path = URL(callFrame->callerSourceOrigin(vm).url(), fileName);
        if (!path.protocolIsFile()) {
            throwException(globalObject, scope, createURIError(globalObject, makeString("caller relative URL path is not a local file: "_s, path.string())));
            return URL();
        }
    } else
        path = absoluteFileURL(fileName);
    return path;
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

static bool fillBufferWithContentsOfFile(const String& fileName, Vector<char>& buffer)
{
    struct stat statBuf;
    auto fileNameUTF = fileName.tryGetUTF8();
    if (!fileNameUTF.has_value()) {
        SAFE_FPRINTF(stderr, "Error when parsing file name: %s\n", fileName.ascii());
        return false;
    }
    if (stat(fileNameUTF->data(), &statBuf) == -1) {
        SAFE_FPRINTF(stderr, "Could not open file: %s\n", *fileNameUTF);
        return false;
    }

    if ((statBuf.st_mode & S_IFMT) != S_IFREG) {
        SAFE_FPRINTF(stderr, "Trying to open a non-file: %s\n", *fileNameUTF);
        return false;
    }
    auto* f = fopen(fileNameUTF->data(), "rb");
    if (!f) {
        SAFE_FPRINTF(stderr, "Could not open file: %s\n", *fileNameUTF);
        return false;
    }

    bool result = fillBufferWithContentsOfFile(f, buffer);
    fclose(f);

    return result;
}

static bool fetchScriptFromLocalFileSystem(const String& fileName, Vector<char>& buffer)
{
    if (!fillBufferWithContentsOfFile(fileName, buffer))
        return false;
    convertShebangToJSComment(buffer);
    return true;
}

class ShellSourceProvider final : public StringSourceProvider {
public:
    static Ref<ShellSourceProvider> create(const String& source, const SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType sourceType)
    {
        return adoptRef(*new ShellSourceProvider(source, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType));
    }

    ~ShellSourceProvider() final
    {
        commitCachedBytecode();
    }

    RefPtr<CachedBytecode> cachedBytecode() const final
    {
        if (!m_cachedBytecode)
            loadBytecode();
        return m_cachedBytecode.copyRef();
    }

    void updateCache(const UnlinkedFunctionExecutable* executable, const SourceCode&, CodeSpecializationKind kind, const UnlinkedFunctionCodeBlock* codeBlock) const final
    {
        if (!cacheEnabled() || !m_cachedBytecode)
            return;
        BytecodeCacheError error;
        RefPtr<CachedBytecode> cachedBytecode = encodeFunctionCodeBlock(executable->vm(), codeBlock, error);
        if (cachedBytecode && !error.isValid())
            m_cachedBytecode->addFunctionUpdate(executable, kind, *cachedBytecode);
    }

    void cacheBytecode(const BytecodeCacheGenerator& generator) const final
    {
        if (!cacheEnabled())
            return;
        if (!m_cachedBytecode)
            m_cachedBytecode = CachedBytecode::create();
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
        const char* cachePath = Options::diskCachePath();
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

        m_cachedBytecode = CachedBytecode::create(WTFMove(*mappedFileData));
    }

    ShellSourceProvider(const String& source, const SourceOrigin& sourceOrigin, String&& sourceURL, const TextPosition& startPosition, SourceProviderSourceType sourceType)
        : StringSourceProvider(source, sourceOrigin, SourceTaintedOrigin::Untainted, WTFMove(sourceURL), startPosition, sourceType)
        // Workers started via $.agent.start are not shut down in a synchronous manner, and it
        // is possible the main thread terminates the process while a worker is writing its
        // bytecode cache, which results in intermittent test failures. As $.agent.start is only
        // a rarely used testing facility, we simply do not cache bytecode on these threads.
        , m_cacheEnabled(true)
    {
    }

    bool cacheEnabled() const { return m_cacheEnabled; }

    mutable RefPtr<CachedBytecode> m_cachedBytecode;
    const bool m_cacheEnabled;
};

static inline SourceCode jscSource(const String& source, const SourceOrigin& sourceOrigin, String sourceURL = String(), const TextPosition& startPosition = TextPosition(), SourceProviderSourceType sourceType = SourceProviderSourceType::Program)
{
    return SourceCode(ShellSourceProvider::create(source, sourceOrigin, WTFMove(sourceURL), startPosition, sourceType), startPosition.m_line.oneBasedInt(), startPosition.m_column.oneBasedInt());
}

template<typename Vector>
static inline String stringFromUTF(const Vector& utf8)
{
    return String::fromUTF8WithLatin1Fallback(utf8.span());
}

template<typename Vector>
static inline SourceCode jscSource(const Vector& utf8, const SourceOrigin& sourceOrigin, const String& filename)
{
    // FIXME: This should use an absolute file URL https://bugs.webkit.org/show_bug.cgi?id=193077
    String str = stringFromUTF(utf8);
    return jscSource(str, sourceOrigin, filename);
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

JSC_DEFINE_HOST_FUNCTION(functionLoad, (JSGlobalObject* globalObject, CallFrame* callFrame))
{
    VM& vm = globalObject->vm();
    auto scope = DECLARE_THROW_SCOPE(vm);

    URL path = computeFilePath(vm, globalObject, callFrame);
    RETURN_IF_EXCEPTION(scope, encodedJSValue());

    Vector<char> script;
    if (!fetchScriptFromLocalFileSystem(path.fileSystemPath(), script))
        return JSValue::encode(throwException(globalObject, scope, createError(globalObject, "Could not open file."_s)));

    NakedPtr<Exception> evaluationException;
    JSValue result = evaluate(globalObject, jscSource(script, SourceOrigin { path }, path.fileSystemPath()), JSValue(), evaluationException);
    if (evaluationException) {
        if (vm.isTerminationException(evaluationException.get()))
            vm.setExecutionForbidden();
        throwException(globalObject, scope, evaluationException);
    }
    return JSValue::encode(result);
}

/*
        Test discovery and execution
*/

static VM& prepareJSCForExecution()
{
    Config::enableRestrictedOptions();
    Options::machExceptionHandlerSandboxPolicy = JSC::Options::SandboxPolicy::Allow;

    WTF::initializeMainThread();
    initialize();

    Options::useDollarVM() = true;

    return VM::create(HeapType::Large).leakRef();
}

static SourceCode createSourceCode(const String& contents)
{
    auto sourceOrigin = SourceOrigin();
    return makeSource(contents, sourceOrigin, SourceTaintedOrigin::Untainted);
}

static Exception* runString(JSGlobalObject* globalObject, String text)
{
    NakedPtr<Exception> exception;
    {
        SourceCode code = createSourceCode(text);
        evaluate(globalObject, code, JSValue(), exception);
    }
    return exception.get();
}

static void defineAssertionBuiltins(JSGlobalObject* globalObject)
{
    auto* exception = runString(globalObject,
        R"(
            const $assert = {

                fail : (msg, extra) => {
                    throw new Error(msg + (extra ? ": " + extra : ""))
                },

                isNotA : (v, t, msg) => {
                    if (typeof v === t)
                        $assert.fail(`Shouldn't be ${t}`, msg);
                },

                isA : (v, t, msg) => {
                    if (typeof v !== t)
                        $assert.fail(`Should be ${t}, got ${typeof(v)}`, msg);
                },

                isNotUndef : (v, msg) => $assert.isNotA(v, "undefined", msg),
                isUndef : (v, msg) => $assert.isA(v, "undefined", msg),
                notObject : (v, msg) => $assert.isNotA(v, "object", msg),
                isObject : (v, msg) => $assert.isA(v, "object", msg),
                notString : (v, msg) => $assert.isNotA(v, "string", msg),
                isString : (v, msg) => $assert.isA(v, "string", msg),
                notNumber : (v, msg) => $assert.isNotA(v, "number", msg),
                isNumber : (v, msg) => $assert.isA(v, "number", msg),
                notFunction : (v, msg) => $assert.isNotA(v, "function", msg),
                isFunction : (v, msg) => $assert.isA(v, "function", msg),

                truthy : (value, msg) => {
                    if (!value) $assert.fail(`Expected truthy "${value}"`, msg);
                },

                falsy : (value, msg) => {
                    if (value) $assert.fail(`Expected falsy "${value}"`, msg);
                },

                eq : (lhs, rhs) => {
                    if (typeof lhs !== typeof rhs)
                        $assert.fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
                    if (Array.isArray(lhs) && Array.isArray(rhs) && (lhs.length === rhs.length)) {
                        for (let i = 0; i !== lhs.length; ++i)
                            eq(lhs[i], rhs[i], msg);
                    } else if (lhs !== rhs) {
                        if (typeof lhs === "number" && isNaN(lhs) && isNaN(rhs))
                            return;
                        $assert.fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
                    } else {
                        if (typeof lhs === "number" && (1.0 / lhs !== 1.0 / rhs)) // Distinguish -0.0 from 0.0.
                            $assert.fail(`Not the same: "${lhs}" and "${rhs}"`, msg);
                    }
                }
            };
        )"_s);
    RELEASE_ASSERT(!exception);
}

struct TestPassed { };
struct TestFailed {
    JSGlobalObject* globalObject;
    Exception* exception;
};
using TestOutcome = Variant<TestPassed, TestFailed>;

static TestOutcome runTest(VM& vm, const Test& test)
{
    JSGlobalObject* globalObject = JSGlobalObject::create(vm, JSGlobalObject::createStructure(vm, jsNull()));

    Identifier functionName = Identifier::fromString(vm, "$probe"_s);
    JSFunction* function = JSFunction::create(vm, globalObject, 0, functionName.string(), functionProbe, ImplementationVisibility::Public);
    globalObject->putDirect(vm, functionName, function, static_cast<unsigned>(PropertyAttribute::DontEnum));

    functionName = Identifier::fromString(vm, "load"_s);
    function = JSFunction::create(vm, globalObject, 1, functionName.string(), functionLoad, ImplementationVisibility::Public);
    globalObject->putDirect(vm, functionName, function, static_cast<unsigned>(PropertyAttribute::DontEnum));

    defineAssertionBuiltins(globalObject);

    currentTestProbe = test.probe;
    Exception* exception = runString(globalObject, String::fromUTF8(test.kickOffCode));
    currentTestProbe = nullptr;
    if (exception)
        return TestFailed { globalObject, exception };
    return TestPassed { };
}

#define DO_PASTE_TOKENS(a, b) a ## b
#define PASTE_TOKENS(a, b) DO_PASTE_TOKENS(a, b)
#define TEST_LINKER_SECTION_START PASTE_TOKENS(__start_, TEST_LINKER_SECTION_NAME)
#define TEST_LINKER_SECTION_STOP PASTE_TOKENS(__stop_, TEST_LINKER_SECTION_NAME)

#if OS(DARWIN)
extern const Test TEST_LINKER_SECTION_START __asm("section$start$__DATA_CONST$" TEST_LINKER_SECTION_NAME_STR);
extern const Test TEST_LINKER_SECTION_STOP __asm("section$end$__DATA_CONST$" TEST_LINKER_SECTION_NAME_STR);
#elif OS(WINDOWS)
// Linker sections on Windows are too different so for now we don't support it.
// However, building on it should still succeed.
#else
extern "C" {
    extern const Test TEST_LINKER_SECTION_START;
    extern const Test TEST_LINKER_SECTION_STOP;
}
#endif

WTF_ALLOW_UNSAFE_BUFFER_USAGE_BEGIN

#if !OS(WINDOWS)
static std::span<const Test> allJSCTests()
{
    return std::span<const Test>(&TEST_LINKER_SECTION_START, &TEST_LINKER_SECTION_STOP);
}
#endif

static bool hasPrefix(const char* str, const char* prefix)
{
    return StringView::fromLatin1(str).startsWith(StringView::fromLatin1(prefix));
}

static Vector<const Test*> selectAndSort(std::span<const Test> tests, const char* filterPrefix)
{
    Vector<const Test*> selectedTests;
    selectedTests.reserveInitialCapacity(tests.size());
    for (auto& test : tests) {
        if (!filterPrefix || hasPrefix(test.name, filterPrefix))
        selectedTests.append(&test);
    }
    std::sort(selectedTests.begin(), selectedTests.end(),
        [](const Test* a, const Test* b) {
            return codePointCompareLessThan(String::fromLatin1(a->name), String::fromLatin1(b->name));
        });
    return selectedTests;
}

void runAllTests(const char* filterPrefix)
{
#if OS(WINDOWS)
    UNUSED_PARAM(filterPrefix);
#else
    VM& vm = prepareJSCForExecution();
    JSLockHolder locker(vm);

    unsigned successCount = 0;
    unsigned failureCount = 0;
    unsigned ignoredCount = 0;

    auto testsToRun = selectAndSort(allJSCTests(), filterPrefix);
    if (filterPrefix)
        SAFE_PRINTF("Running %zu tests with prefix '%s'\n\n", testsToRun.size(), String::fromLatin1(filterPrefix).utf8());
    else
        SAFE_PRINTF("Running all %zu tests\n\n", testsToRun.size());

    for (const Test* test : testsToRun) {
        SAFE_PRINTF("- %s... ", String::fromLatin1(test->name).utf8());
        fflush(stdout);
        if (test->isIgnored) {
            SAFE_PRINTF("ignored\n");
            ++ignoredCount;
            continue;
        }
        TestOutcome outcome = runTest(vm, *test);
        WTF::switchOn(outcome,
            [&](TestPassed) {
                SAFE_PRINTF("ok\n");
                ++successCount;
            },
            [&](TestFailed failure) {
                SAFE_PRINTF("FAILED\n");
                ++failureCount;
                dumpException(failure.globalObject, failure.exception->value());
            }
        );
    }

    SAFE_PRINTF("\nDONE\n");
    SAFE_PRINTF("%u passed\n%u failed\n", successCount, failureCount);
    SAFE_PRINTF("%u ignored\n", ignoredCount);
#endif
}

} // namespace JSC

/*
        main()
*/

namespace fs = std::filesystem;

static void explain(const char* name)
{
    SAFE_PRINTF("Usage: %s [filterPrefix]\n", String::fromLatin1(name).utf8());
}

int main(int argc, char* argv[])
{
    // Change CWD to the executable's location so unittestScripts/ are found where expected.
    fs::path execPath(argv[0]);
    fs::path execDir = execPath.parent_path();
    if (!execDir.empty())
        fs::current_path(execDir);

    const char* filterPrefix = nullptr;
    switch (argc) {
    case 1:
        break;
    case 2:
        filterPrefix = argv[1];
        break;
    case 0:
        return 2; // should never happen
    default:
        explain(execPath.filename().string().c_str());
        return 1;
    }

    JSC::runAllTests(filterPrefix);
    return 0;
}

WTF_ALLOW_UNSAFE_BUFFER_USAGE_END

#else // !OS(WINDOWS)

int main(int, char**)
{
    SAFE_PRINTF("Skipping all tests because we don't currently support testing on Windows.\n");
    return 0;
}

#endif // !OS(WINDOWS)
