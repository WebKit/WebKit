/*
 * Copyright (c) 2026 Apple Inc. All rights reserved.
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

#import <Foundation/Foundation.h>
#import <TargetConditionals.h>
#import <XCTest/XCTest.h>

// This bundle drives test_pas as a subprocess via NSTask, which is macOS-only
// (iOS sandboxing forbids spawning arbitrary executables). On iOS the file
// compiles to an empty translation unit, so the bundle still builds cleanly
// but registers zero test classes. Running libpas tests on iOS is a separate
// effort — historically handled by build.sh's tar-and-ssh path in
// test-impl.sh, not by xctest.
#if TARGET_OS_OSX

// Each LIBPAS_SUITE() below produces a distinct XCTestCase subclass. xctest
// parallelizes work at the test-class level — when xcodebuild is invoked with
// -parallel-testing-worker-count N, each runner takes a disjoint subset of the
// classes. With one class per suite, all 33 suites can fan out across runners.
//
// The suite filter passed to test_pas is a substring match against
// Test::fullName() in TestHarness.cpp, which has the form
// "(idx):path:line/SUITE/scope/.../testname". Bracketing the suite name with
// '/'s isolates exactly that suite's tests.
//
// The LIBPAS_SUITE() list at the bottom MUST stay in sync with the ADD_SUITE()
// block in src/test/TestHarness.cpp. Mismatches surface as missing test
// classes, not silent regressions.

@interface LibpasSuite : XCTestCase
@end

@implementation LibpasSuite

+ (BOOL)isParallelizable
{
    return YES;
}

- (NSString *)libpas_suiteName
{
    // Subclasses override.
    return nil;
}

- (int)libpas_childProcesses
{
    // Default: serialize within test_pas (xctest fans out across suites).
    // Subclasses can override to give a single suite its own internal
    // parallelism — useful for chaos-style suites that benefit from running
    // many short scenarios concurrently inside one test_pas invocation.
    return 0;
}

- (NSURL *)libpas_testPasURL
{
    NSURL *bundleDir = [[NSBundle bundleForClass:[self class]].bundleURL URLByDeletingLastPathComponent];
    return [bundleDir URLByAppendingPathComponent:@"test_pas"];
}

- (void)test_run
{
    NSString *suite = [self libpas_suiteName];
    if (!suite) {
        // Skip the abstract base class when xctest enumerates it.
        return;
    }

    NSURL *binary = [self libpas_testPasURL];
    XCTAssertTrue([NSFileManager.defaultManager isExecutableFileAtPath:binary.path],
                  @"test_pas not found or not executable at %@", binary.path);

    // Use the test class name, not the suite filter, for the temp file path:
    // LIBPAS_SUITE_FILTER lets the filter contain '/' (sub-scope drill), and
    // that breaks stringByAppendingPathComponent: into a nonexistent directory.
    NSString *className = NSStringFromClass([self class]);
    NSString *tmpPath = [NSTemporaryDirectory() stringByAppendingPathComponent:
                         [NSString stringWithFormat:@"%@-%@.log", className, NSUUID.UUID.UUIDString]];
    [NSFileManager.defaultManager createFileAtPath:tmpPath contents:nil attributes:nil];
    NSFileHandle *log = [NSFileHandle fileHandleForWritingAtPath:tmpPath];
    XCTAssertNotNil(log, @"failed to open temp log %@", tmpPath);

    NSTask *task = [[NSTask alloc] init];
    task.executableURL = binary;
    NSString *childProcessesArg = [NSString stringWithFormat:@"%d", [self libpas_childProcesses]];
    task.arguments = @[ @"--child-processes", childProcessesArg,
                        [NSString stringWithFormat:@"/%@/", suite] ];
    NSMutableDictionary<NSString *, NSString *> *env = [NSProcessInfo.processInfo.environment mutableCopy];
    env[@"DYLD_LIBRARY_PATH"] = [binary URLByDeletingLastPathComponent].path;
    task.environment = env;
    task.standardOutput = log;
    task.standardError = log;

    NSError *launchError = nil;
    if (![task launchAndReturnError:&launchError]) {
        [log closeFile];
        [NSFileManager.defaultManager removeItemAtPath:tmpPath error:nil];
        XCTFail(@"failed to launch test_pas for /%@/: %@", suite, launchError);
        return;
    }
    [task waitUntilExit];
    [log closeFile];

    int status = task.terminationStatus;
    NSTaskTerminationReason reason = task.terminationReason;

    if (status != 0 || reason != NSTaskTerminationReasonExit) {
        NSData *captured = [NSData dataWithContentsOfFile:tmpPath] ?: [NSData data];

        // xctest has this annoying behavior where it seems to trim anything after
        // the first 64KiB of a log; some of our tests will fill that up even when
        // they succeed, much less when they start logging errors.
        // So we need to fetch the full log and attach it as a sidecar in the .xcresult,
        // in the case that the XCTFail message gets trimmed.
        XCTAttachment *attachment = [XCTAttachment attachmentWithData:captured
                                                uniformTypeIdentifier:@"public.plain-text"];
        attachment.lifetime = XCTAttachmentLifetimeKeepAlways;
        attachment.name = [NSString stringWithFormat:@"test_pas-%@.log", className];
        [self addAttachment:attachment];

        const NSUInteger cap = 64 * 1024;
        NSData *tail = (captured.length > cap)
                           ? [captured subdataWithRange:NSMakeRange(captured.length - cap, cap)]
                           : captured;
        NSString *tailStr = [[NSString alloc] initWithData:tail encoding:NSUTF8StringEncoding] ?: @"<non-utf8 output>";
        NSString *reasonStr = (reason == NSTaskTerminationReasonUncaughtSignal) ? @"signal" : @"exit";
        XCTFail(@"test_pas /%@/ failed (%@ %d)\n----- last %lu of %lu bytes (full log attached) -----\n%@",
                suite, reasonStr, status, (unsigned long)tail.length, (unsigned long)captured.length, tailStr);
    }

    [NSFileManager.defaultManager removeItemAtPath:tmpPath error:nil];
}

@end

// SIMPLE_LIBPAS_SUITE is for the common case where the test_pas filter is just
// the ADD_SUITE name and is a valid ObjC identifier — no in-test_pas
// parallelism. LIBPAS_SUITE is the full form: pass an identifier-safe class
// suffix, the filter string (which may contain '/' to drill into a sub-scope),
// and N for --child-processes. Pass 0 for the default no-internal-parallelism
// behavior; pass >0 to give a long suite its own internal parallelism.
// LIBPAS_SUITE_FLAKY tolerates failures by wrapping test_run in an
// XCTExpectFailureWithOptions call configured with non-strict options, so
// either failure or success is reported as a passing test.
#define SIMPLE_LIBPAS_SUITE(NAME) LIBPAS_SUITE(NAME, #NAME, 0)
#define LIBPAS_SUITE(IDENT, FILTER, N)                                \
    @interface Libpas_##IDENT : LibpasSuite @end                      \
    @implementation Libpas_##IDENT                                    \
    - (NSString *)libpas_suiteName { return @FILTER; }                \
    - (int)libpas_childProcesses { return N; }                        \
    @end
#define LIBPAS_SUITE_FLAKY(IDENT, FILTER, N, REASON)                  \
    @interface Libpas_##IDENT : LibpasSuite @end                      \
    @implementation Libpas_##IDENT                                    \
    - (NSString *)libpas_suiteName { return @FILTER; }                \
    - (int)libpas_childProcesses { return N; }                        \
    - (void)test_run                                                  \
    {                                                                 \
        XCTExpectFailureWithOptions(@REASON,                          \
            [XCTExpectedFailureOptions nonStrictOptions]);            \
        [super test_run];                                             \
    }                                                                 \
    @end

// We split up a few of the longest suites to improve parallelism and make it easier
// to track down failures.
LIBPAS_SUITE(ThingyAndUtilityHeapAllocation_WithVerifier, "ThingyAndUtilityHeapAllocation/install-verifier", 0)
LIBPAS_SUITE(ThingyAndUtilityHeapAllocation_SansVerifier, "ThingyAndUtilityHeapAllocation/no-verifier", 0)
SIMPLE_LIBPAS_SUITE(BitfieldVector)
SIMPLE_LIBPAS_SUITE(Bitfit)
SIMPLE_LIBPAS_SUITE(Bitvector)
SIMPLE_LIBPAS_SUITE(Bmalloc)
SIMPLE_LIBPAS_SUITE(CartesianTree)
SIMPLE_LIBPAS_SUITE(Coalign)
SIMPLE_LIBPAS_SUITE(Enumeration)
SIMPLE_LIBPAS_SUITE(ExpendableMemory)
SIMPLE_LIBPAS_SUITE(ExtendedGCD)
SIMPLE_LIBPAS_SUITE(Hashtable)
SIMPLE_LIBPAS_SUITE(HeapRefAllocatorIndex)
SIMPLE_LIBPAS_SUITE(IsoDynamicPrimitiveHeap)
LIBPAS_SUITE_FLAKY(IsoHeapChaos_EnablePageBalancing, "IsoHeapChaos/enable-page-balancing", 4, "rdar://179251452 — rarely crashes")
LIBPAS_SUITE_FLAKY(IsoHeapChaos_DisablePageBalancing, "IsoHeapChaos/disable-page-balancing", 4, "rdar://179251452 — rarely crashes")
SIMPLE_LIBPAS_SUITE(IsoHeapPageSharing)
SIMPLE_LIBPAS_SUITE(IsoHeapReservedMemory)
SIMPLE_LIBPAS_SUITE(JITHeap)
SIMPLE_LIBPAS_SUITE(LargeFreeHeap)
SIMPLE_LIBPAS_SUITE(LargeSharingPool)
SIMPLE_LIBPAS_SUITE(LockFreeReadPtrPtrHashtable)
SIMPLE_LIBPAS_SUITE(LotsOfHeapsAndThreads)
SIMPLE_LIBPAS_SUITE(MAR)
SIMPLE_LIBPAS_SUITE(Memalign)
SIMPLE_LIBPAS_SUITE(MinHeap)
SIMPLE_LIBPAS_SUITE(PGM)
SIMPLE_LIBPAS_SUITE(Race)
SIMPLE_LIBPAS_SUITE(RedBlackTree)
SIMPLE_LIBPAS_SUITE(ReallocFastPath)
SIMPLE_LIBPAS_SUITE(ScavengerExternalWork)
LIBPAS_SUITE_FLAKY(TLCDecommit, "TLCDecommit", 0, "rdar://86924027 — broken pending some accounting fixes from the kernel")
SIMPLE_LIBPAS_SUITE(TSD)
SIMPLE_LIBPAS_SUITE(Utils)
SIMPLE_LIBPAS_SUITE(ViewCache)


#undef SIMPLE_LIBPAS_SUITE
#undef LIBPAS_SUITE
#undef LIBPAS_SUITE_FLAKY

#endif // TARGET_OS_OSX
