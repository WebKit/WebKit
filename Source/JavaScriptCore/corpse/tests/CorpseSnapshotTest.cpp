/*
 * Copyright (C) 2026 Apple Inc. All rights reserved.
 * Copyright (C) 2026 Igalia S.L.
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

#if ENABLE(MYA)

#include "LibJSCToolsTestUtilities.h"

#include <JavaScriptCore/CorpseImage.h>
#include <JavaScriptCore/CorpseProcess.h>
#include <JavaScriptCore/CorpseSnapshot.h>
#include <JavaScriptCore/JSContextRef.h>
#include <algorithm>
#include <dlfcn.h>
#include <errno.h>
#include <pthread.h>
#include <signal.h>
#include <stdlib.h>
#include <string_view>
#include <sys/stat.h>
#include <sys/wait.h>
#if OS(DARWIN)
#include <libproc.h>
#include <mach-o/dyld.h>
#include <mach-o/loader.h>
#include <mach/mach.h>
#include <sys/proc.h>
#else
#include <link.h>
#include <sys/auxv.h>
#include <wtf/posix/POSIXExtras.h>
#endif
#include <unistd.h>
#include <wtf/FileSystem.h>
#include <wtf/HashSet.h>
#include <wtf/HexNumber.h>
#include <wtf/Scope.h>
#include <wtf/text/MakeString.h>
#include <wtf/text/StringBuilder.h>
#if ENABLE(MYA_HEAP)
#include <lldb/API/LLDB.h>
#endif

namespace JSCToolsTest {

using JSC::Corpse::Address;
using JSC::Corpse::Image;
using JSC::Corpse::Process;
using JSC::Corpse::Snapshot;
using JSC::Corpse::TaskHandle;

static const Image* imageAt(const Vector<Image>& images, Address loadAddress)
{
    auto found = std::ranges::find(images, loadAddress, &Image::loadAddress);
    return found == images.end() ? nullptr : &*found;
}

static const Image* imageContaining(const Vector<Image>& images, const void* function)
{
    Dl_info info;
    if (!dladdr(function, &info) || !info.dli_fbase) {
        TEST_ASSERT(false, "dladdr finds the image of every function the test looks for");
        return nullptr;
    }
    return imageAt(images, Address { info.dli_fbase });
}

static bool isSameFile(const char* path, const char* other)
{
    struct stat file;
    struct stat otherFile;
    return !stat(path, &file) && !stat(other, &otherFile) && file.st_dev == otherFile.st_dev && file.st_ino == otherFile.st_ino;
}

static bool listsEachImageOnce(const Vector<Image>& images)
{
    HashSet<uint64_t> headers;
    return std::ranges::all_of(images, [&](const Image& image) {
        return headers.add(image.loadAddress().toTargetVMAddress()).isNewEntry;
    });
}

#if ENABLE(MYA_HEAP)
// What Image's comment says liblldb needs: each image loads by its path and UUID at
// its slide, and the functions this process calls resolve to their symbols.
static void testImagesLoadIntoLLDB(const Vector<Image>& images)
{
    lldb::SBDebugger::Initialize();
    auto terminateLLDB = makeScopeExit([] {
        lldb::SBDebugger::Terminate();
    });
    lldb::SBDebugger debugger = lldb::SBDebugger::Create();
    auto destroyDebugger = makeScopeExit([&] {
        lldb::SBDebugger::Destroy(debugger);
    });
    lldb::SBTarget target = debugger.CreateTarget("");
    TEST_ASSERT(target.IsValid(), "liblldb makes an empty target");
    if (!target.IsValid())
        return;
    auto deleteTarget = makeScopeExit([&] {
        debugger.DeleteTarget(target);
    });

    unsigned notLoaded = 0;
    for (const Image& image : images) {
        StringBuilder uuid;
        for (uint8_t byte : image.uuid())
            uuid.append(hex(byte, 2));
        lldb::SBModule module = target.AddModule(image.path().legacyCStringPointer(), nullptr, uuid.isEmpty() ? nullptr : uuid.toString().utf8().legacyCStringPointer());
        lldb::addr_t headerFileAddress = module.IsValid() ? module.GetObjectFileHeaderAddress().GetFileAddress() : LLDB_INVALID_ADDRESS;
        if (headerFileAddress == LLDB_INVALID_ADDRESS
            || !target.SetModuleLoadAddress(module, image.loadAddress().toTargetVMAddress() - headerFileAddress).Success()) {
            dataLogLn("    liblldb did not load ", image.path().legacyCStringPointer());
            ++notLoaded;
        }
    }
    TEST_ASSERT_EQ(notLoaded, 0u, "liblldb loads every image at its slide");

    auto symbolAt = [&](const void* function) {
        lldb::SBAddress address = target.ResolveLoadAddress(Address { function }.stripped().toTargetVMAddress());
        const char* name = address.GetSymbol().GetName();
        return std::string_view { name ? name : "" };
    };
    TEST_ASSERT(symbolAt(reinterpret_cast<const void*>(&testSnapshot)).contains("testSnapshot"), "liblldb resolves a function of this executable");
    TEST_ASSERT(symbolAt(reinterpret_cast<const void*>(&JSGlobalContextCreate)).contains("JSGlobalContextCreate"), "and of JavaScriptCore");
#if !OS(DARWIN)
    TEST_ASSERT(symbolAt(reinterpret_cast<const void*>(&getpid)).contains("getpid"), "and of libc");
#endif
}
#endif

static void testImages(RefPtr<Process> process)
{
    Snapshot snapshot(process);
    const Vector<Image>& images = snapshot.images();
    TEST_ASSERT(!images.isEmpty(), "a snapshot lists the images of its process");
    TEST_ASSERT(&snapshot.images() == &images, "the image list is read once");

    const Image* executable = imageContaining(images, reinterpret_cast<const void*>(&testSnapshot));
    TEST_ASSERT(executable, "the image list has this executable, at the address the loader put it");
    const Image* javaScriptCore = imageContaining(images, reinterpret_cast<const void*>(&JSGlobalContextCreate));
    TEST_ASSERT(javaScriptCore, "the image list has JavaScriptCore, at the address the loader put it");

    // The loader's own list, read in process, says which images there are.
    // The vDSO and the shared cache's images are checked to be left out.
    unsigned loaded = 0;
    unsigned missing = 0;
    unsigned excluded = 0;
    unsigned excludedButListed = 0;
#if OS(DARWIN)
    for (uint32_t index = 0; index < _dyld_image_count(); ++index) {
        const mach_header* header = _dyld_get_image_header(index);
        if (header->flags & MH_DYLIB_IN_CACHE) {
            ++excluded;
            if (imageAt(images, Address { header })) {
                dataLogLn("    listed from the shared cache: ", _dyld_get_image_name(index));
                ++excludedButListed;
            }
            continue;
        }
        ++loaded;
        const Image* image = imageAt(images, Address { header });
        if (!image || std::string_view(image->path().legacyCStringPointer()) != std::string_view(_dyld_get_image_name(index))) {
            dataLogLn("    not listed: ", _dyld_get_image_name(index), image ? " as " : "", image ? image->path().legacyCStringPointer() : "");
            ++missing;
        }
    }
#else
    struct LoadedImages {
        const Vector<Image>& images;
        unsigned& loaded;
        unsigned& missing;
        unsigned& excluded;
        unsigned& excludedButListed;
    } loadedImages { images, loaded, missing, excluded, excludedButListed };
    dl_iterate_phdr([](dl_phdr_info* info, size_t, void* context) {
        auto& loadedImages = *static_cast<LoadedImages*>(context);
        // The vDSO has no file to read it from.
        uintptr_t vdso = getauxval(AT_SYSINFO_EHDR);
        uintptr_t programHeaders = reinterpret_cast<uintptr_t>(info->dlpi_phdr);
        if (vdso && programHeaders >= vdso && programHeaders < vdso + static_cast<uintptr_t>(getpagesize())) {
            ++loadedImages.excluded;
            if (imageAt(loadedImages.images, Address { static_cast<uint64_t>(vdso) })) {
                dataLogLn("    listed: the vDSO");
                ++loadedImages.excludedButListed;
            }
            return 0;
        }
        ++loadedImages.loaded;
        // An image's program headers are in the mapping of its file that holds its ELF header.
        Dl_info header;
        const Image* image = dladdr(info->dlpi_phdr, &header) ? imageAt(loadedImages.images, Address { header.dli_fbase }) : nullptr;
        // Only the executable has no name here.
        const char* path = *info->dlpi_name ? info->dlpi_name : "/proc/self/exe";
        if (!image || !isSameFile(image->path().legacyCStringPointer(), path)) {
            dataLogLn("    not listed: ", path, image ? " (another path)" : "");
            ++loadedImages.missing;
        }
        return 0;
    }, &loadedImages);
#endif
    TEST_ASSERT_EQ(missing, 0u, "every image the loader lists is listed, with the loader's path");
    TEST_ASSERT_EQ(images.size(), static_cast<size_t>(loaded), "and no other");
    TEST_ASSERT(excluded, "the loader lists an image that is left out: the vDSO, or the shared cache's");
    TEST_ASSERT_EQ(excludedButListed, 0u, "which is not listed");
    TEST_ASSERT(listsEachImageOnce(images), "no image is listed twice");

#if OS(DARWIN)
    unsigned withoutUUID = 0;
    for (const Image& image : images) {
        if (std::ranges::all_of(image.uuid(), [](uint8_t byte) { return !byte; }))
            ++withoutUUID;
    }
    TEST_ASSERT_EQ(withoutUUID, 0u, "every image has a UUID");
    if (executable && javaScriptCore)
        TEST_ASSERT(!std::ranges::equal(executable->uuid(), javaScriptCore->uuid()), "two images have two UUIDs");
    TEST_ASSERT(!imageContaining(images, reinterpret_cast<const void*>(&malloc)),
        "a shared-cache image is not listed");
#else
    TEST_ASSERT(std::ranges::all_of(images, [](const Image& image) { return image.uuid().empty(); }),
        "a Linux image has no UUID: liblldb reads the build-id from the file");
    TEST_ASSERT(imageContaining(images, reinterpret_cast<const void*>(&malloc)),
        "the image list has libc");
#endif
#if ENABLE(MYA_HEAP)
    testImagesLoadIntoLLDB(images);
#else
    dataLogLn("SKIP: Snapshot images in liblldb: built without liblldb");
#endif
}

static uint64_t imageTargetGlobal;

static Address createImageTarget()
{
    return Address { &imageTargetGlobal };
}

static bool namesThisExecutable(const Image& image)
{
    UTF8CString executablePath = Process::create(getpid())->executablePath();
    return image.path().span().front() == '/' && isSameFile(image.path().legacyCStringPointer(), executablePath.legacyCStringPointer());
}

static void analyzeTargetImages(Snapshot& snapshot, Address)
{
    TEST_ASSERT(std::ranges::count_if(snapshot.images(), namesThisExecutable) == 1, "the target's executable is listed once");
}

static bool isReplacedCopy(const Image& image)
{
    return std::string_view(image.path().legacyCStringPointer()).contains(".replaced-");
}

static void analyzeReplacedExecutable(Snapshot& snapshot, Address)
{
#if OS(DARWIN)
    // The copy has this executable's bytes, so it was loaded with this executable's UUID.
    auto self = takeSnapshot(getpid());
    const Image* original = self ? imageContaining(self->images(), reinterpret_cast<const void*>(&testSnapshot)) : nullptr;
    const Vector<Image>& images = snapshot.images();
    auto replaced = std::ranges::find_if(images, isReplacedCopy);
    TEST_ASSERT(original && replaced != images.end() && std::ranges::equal(replaced->uuid(), original->uuid()),
        "a replaced executable is listed, with the UUID it was loaded with");
#else
    ExpectedErrors expectedErrors;
    const Vector<Image>& images = snapshot.images();
    TEST_ASSERT(!images.isEmpty(), "the images of a target whose executable was replaced are listed");
    TEST_ASSERT(std::ranges::none_of(images, isReplacedCopy), "but not the executable, whose path names another file");
#endif
}

#if OS(LINUX)
static constexpr ASCIILiteral relativeLibraryName = "librelative.so"_s;

// The directory the target made, holding a copy of a system library.
static std::array<char, 64> relativeLibraryDirectory;

// The target dlopens the copy by a path relative to its own working directory,
// which ld.so keeps as the image's name.
static Address createRelativeLibrary()
{
    void* system = dlopen("libanl.so.1", RTLD_NOW);
    link_map* map = nullptr;
    RELEASE_ASSERT(system && !dlinfo(system, RTLD_DI_LINKMAP, &map) && map->l_name && *map->l_name);
    std::ranges::copy(std::string_view { "/tmp/mya-relative-XXXXXX" }, relativeLibraryDirectory.begin());
    RELEASE_ASSERT(mkdtemp(relativeLibraryDirectory.data()));
    String directory = String::fromUTF8(relativeLibraryDirectory.data());
    RELEASE_ASSERT(FileSystem::copyFile(FileSystem::pathByAppendingComponent(directory, relativeLibraryName), String::fromUTF8(map->l_name)));
    RELEASE_ASSERT(!chdir(relativeLibraryDirectory.data()));
    RELEASE_ASSERT(posixDlopen(makeString("./"_s, relativeLibraryName).utf8(), RTLD_NOW));
    return Address { relativeLibraryDirectory.data() };
}

static void analyzeRelativeLibrary(Snapshot& snapshot, Address directoryAddress)
{
    auto directory = snapshot.memory().cString(directoryAddress, relativeLibraryDirectory.size());
    TEST_ASSERT(directory, "the target reports the directory of its library");
    if (!directory)
        return;
    String directoryPath = String::fromUTF8(directory->span());
    String library = FileSystem::pathByAppendingComponent(directoryPath, relativeLibraryName);
    auto removeLibrary = makeScopeExit([&] {
        FileSystem::deleteFile(library);
        FileSystem::deleteEmptyDirectory(directoryPath);
    });
    UTF8CString libraryPath = library.utf8();
    TEST_ASSERT(std::ranges::count_if(snapshot.images(), [&](const Image& image) {
        return image.path().span().front() == '/' && isSameFile(image.path().legacyCStringPointer(), libraryPath.legacyCStringPointer());
    }) == 1, "a library loaded by a path relative to the target's working directory is listed, by its absolute path");
}
#endif

#if OS(LINUX)
// A library a target loaded in a linker namespace of its own, and how ld.so names it.
struct HiddenLibrary {
    uint64_t header { 0 };
    std::array<char, 256> path { };
};
static HiddenLibrary hiddenLibrary;

static void recordHiddenLibrary(void* handle)
{
    link_map* map = nullptr;
    Dl_info header;
    RELEASE_ASSERT(handle && !dlinfo(handle, RTLD_DI_LINKMAP, &map) && dladdr(map->l_ld, &header));
    hiddenLibrary.header = reinterpret_cast<uintptr_t>(header.dli_fbase);
    std::string_view path { map->l_name };
    RELEASE_ASSERT(path.size() < hiddenLibrary.path.size());
    std::ranges::copy(path, hiddenLibrary.path.begin());
}

static Address createLibraryInNewLinkerNamespace()
{
    recordHiddenLibrary(dlmopen(LM_ID_NEWLM, "libanl.so.1", RTLD_NOW));
    return Address { &hiddenLibrary };
}

static void analyzeLibraryInNewLinkerNamespace(Snapshot& snapshot, Address fixture)
{
    auto library = snapshot.memory().ptr<HiddenLibrary>(fixture);
    TEST_ASSERT(library && library->header, "the target reports the library it loaded in a new linker namespace");
    if (!library)
        return;
    const Image* image = imageAt(snapshot.images(), Address { library->header });
    TEST_ASSERT(image && isSameFile(image->path().legacyCStringPointer(), library->path.data()), "a library dlmopen loaded in a new linker namespace is listed");
    TEST_ASSERT(listsEachImageOnce(snapshot.images()), "and no image of either namespace is listed twice");
}

#endif

static void testTargetImages()
{
    analyzeInAndOutOfProcess(createImageTarget, analyzeTargetImages);
    analyzeInSeparateProcessStartedByRelativePath(createImageTarget, analyzeTargetImages);
    analyzeAfterExecutableReplaced(createImageTarget, analyzeReplacedExecutable);
#if OS(LINUX)
    TEST_ASSERT(analyzeInSeparateProcess(createRelativeLibrary, analyzeRelativeLibrary), "a target that loads a library by a relative path is snapshotted");
    TEST_ASSERT(analyzeInSeparateProcess(createLibraryInNewLinkerNamespace, analyzeLibraryInNewLinkerNamespace), "a target with two linker namespaces is snapshotted");
#endif
}

// A global the target changes after the snapshot is taken.
static volatile uint64_t changingGlobal;

static Address createChangingGlobal()
{
    changingGlobal = 1;
    reportTargetObject(Address { const_cast<uint64_t*>(&changingGlobal) });
    for (;;) {
        changingGlobal = changingGlobal + 1;
        usleep(1000);
    }
}

static uint64_t setBeforeOffer;

static Address createLateOffer()
{
    setBeforeOffer = 0;
    reportTargetAddress(Address { &setBeforeOffer });
    usleep(100 * 1000);
    setBeforeOffer = 1;
    offerTargetSnapshot();
    for (;;)
        pause();
}

// A Linux copy would die with the thread that forked it, so offer() refuses any but the main thread.
static Address createOfferOffMainThread()
{
    reportTargetAddress(Address { &setBeforeOffer });
    pthread_t thread;
    RELEASE_ASSERT(!pthread_create(&thread, nullptr, [](void*) -> void* {
        offerTargetSnapshot();
        return nullptr;
    }, nullptr));
    pthread_join(thread, nullptr);
    for (;;)
        pause();
}

static void analyzeLateOffer(Snapshot& snapshot, Address flag)
{
    auto value = snapshot.memory().ptr<uint64_t>(flag);
    TEST_ASSERT(value && *value == 1, "a snapshot waits until the target offers it");
}

// Whether `pid` is alive and not stopped within a second.
static bool runsOn(pid_t pid)
{
    for (unsigned attempt = 0; attempt < 100; ++attempt) {
#if OS(DARWIN)
        struct proc_bsdinfo info;
        bool running = proc_pidinfo(pid, PROC_PIDTBSDINFO, 0, &info, sizeof(info)) == sizeof(info) && info.pbi_status != SSTOP && info.pbi_status != SZOMB;
#else
        auto stat = JSC::Corpse::readProcFile(pid, "stat");
        size_t end = stat ? stat->rfind(')') : std::string::npos;
        char state = end != std::string::npos && end + 2 < stat->size() ? (*stat)[end + 2] : 'X';
        bool running = state == 'R' || state == 'S';
#endif
        if (running)
            return true;
        usleep(10 * 1000);
    }
    return false;
}

static void analyzeChangingGlobal(Snapshot& snapshot, Address global)
{
    auto before = snapshot.memory().ptr<uint64_t>(global);
    usleep(20 * 1000);
    auto after = snapshot.memory().ptr<uint64_t>(global);
    TEST_ASSERT(before && after && *before == 1 && *after == 1, "a global the target changes after the snapshot reads as it was");
    TEST_ASSERT(runsOn(snapshot.process()->pid()), "and the target runs on once it is snapshotted");
}

static void testCopies()
{
    {
        changingGlobal = 1;
        auto snapshot = takeSnapshot(getpid());
        changingGlobal = 2;
        if (snapshot) {
            auto value = snapshot->memory().ptr<uint64_t>(Address { const_cast<uint64_t*>(&changingGlobal) });
            TEST_ASSERT(value && *value == 1, "a global this process changes after the snapshot reads as it was");
        }
    }
    TEST_ASSERT(analyzeInSeparateProcess(createChangingGlobal, analyzeChangingGlobal), "a target that changes a global is snapshotted");
    TEST_ASSERT(analyzeInSeparateProcess(createLateOffer, analyzeLateOffer), "a target that offers late is snapshotted");

    {
        ExpectedErrors expectedErrors;
        TEST_ASSERT(!analyzeInSeparateProcess(createOfferOffMainThread, [](Snapshot&, Address) { }), "a target that offers off its main thread crashes, and is not snapshotted");
    }

#if OS(LINUX)
    // A copy lives as long as its snapshot, and is reaped with it.
    pid_t firstCopy = 0;
    pid_t secondCopy = 0;
    {
        auto second = takeSnapshot(getpid());
        {
            auto first = takeSnapshot(getpid());
            if (!first || !second)
                return;
            firstCopy = first->corpsePort();
            secondCopy = second->corpsePort();
            TEST_ASSERT(firstCopy != secondCopy && firstCopy != getpid() && !kill(firstCopy, 0), "a snapshot of this process is a live copy of it");
        }
        TEST_ASSERT(kill(firstCopy, 0) && errno == ESRCH, "releasing a snapshot ends its copy, and reaps it");
        TEST_ASSERT(!kill(secondCopy, 0), "and leaves the copy of another snapshot");
    }
    TEST_ASSERT(kill(secondCopy, 0) && errno == ESRCH, "which ends with its own snapshot");

    // Out of process, the copy is the target's child, so an ended copy may stay its zombie.
    auto waitUntilEnded = [](pid_t copy) {
        for (unsigned attempt = 0; attempt < 1000; ++attempt) {
            auto stat = JSC::Corpse::readProcFile(copy, "stat");
            size_t end = stat ? stat->rfind(')') : std::string::npos;
            if (!stat || (end != std::string::npos && end + 2 < stat->size() && (*stat)[end + 2] == 'Z'))
                return true;
            usleep(10 * 1000);
        }
        return false;
    };
    analyzeOutOfProcessThenRelease(createChangingGlobal, analyzeChangingGlobal, [&](pid_t, pid_t copy) {
        TEST_ASSERT(waitUntilEnded(copy), "releasing a snapshot of another process ends its copy");
    });
    analyzeAfterTargetExits(createChangingGlobal, [&](Snapshot& snapshot, Address) {
        TEST_ASSERT(waitUntilEnded(snapshot.corpsePort()), "a copy ends with its target");
    });
#endif
}

void testSnapshot()
{
    SuiteTracer tracer("Snapshot");
    if (!tracer.shouldRun())
        return;

    RefPtr<Process> process = Process::create(getpid());
    if (!process->attach()) {
        TEST_ASSERT(false, "attaching to this process succeeds");
        return;
    }

    unsigned firstId = 0;
    TaskHandle firstCorpsePort = JSC::Corpse::invalidTaskHandle;
    TaskHandle secondCorpsePort = JSC::Corpse::invalidTaskHandle;
    {
        Snapshot snapshot(process);
        TEST_ASSERT(snapshot.isValid(), "a snapshot of this process is valid");
        TEST_ASSERT(JSC::Corpse::isValidTaskHandle(snapshot.corpsePort()), "a valid snapshot holds a corpse port");
        TEST_ASSERT(snapshot.process() == process.get(), "a snapshot keeps the process it came from");
        firstId = snapshot.id();
        TEST_ASSERT(firstId, "a snapshot has an identifier");
        firstCorpsePort = snapshot.corpsePort();

        Snapshot second(process);
        TEST_ASSERT(second.isValid(), "a second snapshot of the same process is valid");
        TEST_ASSERT(second.id() > firstId, "identifiers increase");
#if OS(DARWIN)
        TEST_ASSERT(second.corpsePort() != snapshot.corpsePort(),
            "two snapshots hold two different corpses");
#endif
        secondCorpsePort = second.corpsePort();
    }
#if OS(DARWIN)
    TEST_ASSERT_EQ(machPortSendRightCount(firstCorpsePort), 0u,
        "destroying a snapshot gives its corpse port back");
    TEST_ASSERT_EQ(machPortSendRightCount(secondCorpsePort), 0u,
        "and so does destroying the second");
#else
    UNUSED_VARIABLE(firstCorpsePort);
    UNUSED_VARIABLE(secondCorpsePort);
#endif
    {
        // The two above are gone; their identifiers must not come back.
        Snapshot later(process);
        TEST_ASSERT(later.id() > firstId + 1, "identifiers are not reused after a snapshot is destroyed");
    }
    {
        ExpectedErrors expectedErrors(4);
        RefPtr<Process> unattached = Process::create(getpid());
        Snapshot snapshot(unattached);
        TEST_ASSERT(!snapshot.isValid(), "a snapshot of an unattached process is invalid");
        TEST_ASSERT(snapshot.threads().isEmpty(), "an invalid snapshot reports no threads");
        TEST_ASSERT(snapshot.images().isEmpty(), "an invalid snapshot reports no images");
        TEST_ASSERT(snapshot.images().isEmpty(), "and reports it once");
        TEST_ASSERT(!snapshot.symbol("g_config"), "an invalid snapshot resolves no symbol");
    }
    {
        ExpectedErrors expectedErrors(3);
        RefPtr<Process> none;
        Snapshot snapshot(none);
        TEST_ASSERT(!snapshot.isValid(), "a snapshot with no process is invalid");
        TEST_ASSERT(!snapshot.symbol("g_config"), "a snapshot with no process resolves no symbol");
        TEST_ASSERT(!snapshot.symbol("g_config"), "nor does it the second time, from the cache");
    }
    {
        Snapshot snapshot(process);
        ExpectedErrors expectedErrors(2);
        TEST_ASSERT(!snapshot.symbol(nullptr), "an unnamed symbol resolves to nothing");
        TEST_ASSERT(!snapshot.symbol(""), "an empty symbol name resolves to nothing");
    }
    {
        // A process that exits before it offers a snapshot offers none.
        pid_t exited = fork();
        if (!exited)
            _exit(0);
        TEST_ASSERT(exited > 0 && waitpid(exited, nullptr, 0) == exited, "a process exits");
        ExpectedErrors expectedErrors(1);
        TEST_ASSERT(!Snapshot::receive(exited), "a process that exited offers no snapshot");
    }
    testImages(process);
    testTargetImages();
    testCopies();

#if OS(DARWIN)
    {
        // A corpse and the thread rights read out of it are Mach ports. Taking a snapshot
        // must not leave any of them behind.
        unsigned namesBefore = machPortNameCount();
        mach_port_t corpsePort = MACH_PORT_NULL;
        {
            Snapshot snapshot(process);
            if (!snapshot.isValid()) {
                TEST_ASSERT(false, "a snapshot of this process is valid");
                return;
            }
            corpsePort = snapshot.corpsePort();
            TEST_ASSERT(machPortSendRightCount(corpsePort), "a snapshot holds a right to its corpse");

            unsigned namesBeforeThreads = machPortNameCount();
            snapshot.threads();
            TEST_ASSERT_EQ(machPortNameCount(), namesBeforeThreads,
                "reading the thread list gives back every thread right it took");
        }

        TEST_ASSERT_EQ(machPortSendRightCount(corpsePort), static_cast<unsigned>(0),
            "destroying a snapshot gives back the right to its corpse");
        TEST_ASSERT_EQ(machPortNameCount(), namesBefore,
            "and leaves no port name behind");
    }
#endif // OS(DARWIN)
}

} // namespace JSCToolsTest

#endif // ENABLE(MYA)
