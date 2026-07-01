/*
 * Copyright (C) 2026 Igalia S.L. All rights reserved.
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

// Runs one JetStream3 subtest under jsc, warming up until the set of JIT-compiled
// functions stops changing, then measures a window in which nothing recompiles or
// changes tier, and prints one "__FTL_ONLY_REPORT__"-prefixed JSON line.
//
// With the compiled set frozen and identical between two builds, a difference in
// compile time / OSR exits / per-iteration runtime is attributable to the compiler
// rather than to how far warmup happened to get. The jsc options that make the set
// deterministic are set by the orchestrator (run-ftl-only-jsc-benchmarks), which is
// the normal way to invoke this; run it directly only to debug one subtest.
//
//   jsc <options> ftl-only-cli.js -- --test=<name> [--measurement-iters=N] [--warmup-budget-mult=N]

if (typeof $vm === "undefined")
    throw new Error("ftl-only-cli.js requires JSC_useDollarVM=true");

const isInBrowser = false;
console = { log: print };

// JetStreamDriver.js expects these shell-detection globals (cli.js sets them too).
const isD8 = typeof Realm !== "undefined";
if (isD8)
    globalThis.readFile = read;
const isSpiderMonkey = typeof newGlobal !== "undefined";
if (isSpiderMonkey)
    globalThis.readFile = readRelativeToScript;

const argv = (typeof arguments !== "undefined") ? arguments : [];
const args = {};
for (const arg of argv) {
    if (typeof arg !== "string" || !arg.startsWith("--"))
        continue;
    const eq = arg.indexOf("=");
    if (eq < 0)
        args[arg.slice(2)] = "true";
    else
        args[arg.slice(2, eq)] = arg.slice(eq + 1);
}

const TEST_NAME = args.test;
if (!TEST_NAME)
    throw new Error("--test=<name> required");

const MEASUREMENT_ITERS  = parseInt(args["measurement-iters"]  || "30", 10);
const WARMUP_BUDGET_MULT = parseInt(args["warmup-budget-mult"] || "10", 10);
// "Frozen" = no DFG/FTL compilation for this many consecutive iterations. We watch
// compile activity rather than requiring every function be FTL, since cold paths
// that never tier up execute too rarely to perturb measurement.
const FREEZE_ITERATIONS_REQUIRED = parseInt(args["freeze-iterations"] || "30", 10);

if (typeof testList === "undefined")
    testList = undefined;
if (typeof testIterationCount === "undefined")
    testIterationCount = undefined;
RAMification = false;

load("./JetStreamDriver.js");

const plan = testPlans.find(p => p.name === TEST_NAME);
if (!plan)
    throw new Error("unknown test: " + TEST_NAME);

const BenchmarkClass = plan.benchmarkClass || DefaultBenchmark;

function reportUnsupported(reason) {
    print("__FTL_ONLY_REPORT__" + JSON.stringify({ test: TEST_NAME, skipped: true, reason }));
    quit();
}

// Only Default/AsyncBenchmark have the warmup-then-measure shape; WSL and Wasm
// benchmarks have fixed phases instead. Worker-group benchmarks run their hot code
// on worker VMs, invisible to this VM's heap walk and not measured by main-thread
// timing, so they neither tier up nor measure meaningfully here.
if (BenchmarkClass !== DefaultBenchmark && BenchmarkClass !== AsyncBenchmark)
    reportUnsupported("benchmarkClass " + BenchmarkClass.name + " is not a warmup/measure benchmark");
if (typeof WorkerTestsGroup !== "undefined" && plan.testGroup === WorkerTestsGroup)
    reportUnsupported("worker-group benchmark: hot code runs off the measured VM");

const isAsync = (BenchmarkClass === AsyncBenchmark);
const warmupBudget = MEASUREMENT_ITERS * WARMUP_BUDGET_MULT;

const benchmark = new BenchmarkClass(plan);

// Construct Benchmark exactly as the stock runnerCode does, so the same code compiles.
const benchmarkConstructorArgs = isAsync ? "" : String(benchmark.iterations);
const runIterationStatement = isAsync ? "await __benchmark.runIteration();"
                                      : "__benchmark.runIteration();";

const runnerCode = `
${isAsync ? "(async function doRun() {" : "(function doRun() {"}
    const __benchmark = new Benchmark(${benchmarkConstructorArgs});

    const MEASUREMENT_ITERS = ${MEASUREMENT_ITERS};
    const WARMUP_BUDGET     = ${warmupBudget};
    const FREEZE_ITERATIONS_REQUIRED = ${FREEZE_ITERATIONS_REQUIRED};

    const dfgFtlCompileMs = () => {
        const t = $vm.compileTimeTotals();
        return t.dfgMs + t.ftlMs;
    };
    const ftlDfgCount = () => {
        const c = $vm.codeBlockTierCounts();
        return c.dfg + c.ftl;
    };

    const oneIteration = ${isAsync ? "async " : ""}() => {
        if (__benchmark.prepareForNextIteration)
            __benchmark.prepareForNextIteration();
        ${benchmark.preiterationCode}
        ${runIterationStatement}
    };

    // Reset so frozen.* excludes the driver/test scripts compiled during load.
    $vm.resetJITStats();

    // Warm up until FREEZE_ITERATIONS_REQUIRED consecutive iterations see no DFG/FTL
    // compilation, then run a measurement-sized verification window. If anything
    // recompiles or changes tier during it, fold it back into warmup and retry; the
    // first clean window IS the measurement.
    let warmupIterations = 0;
    let reachedFrozenState = false;
    let consecutiveFrozenIterations = 0;
    let verificationsTried = 0;
    let prevCompileMs = dfgFtlCompileMs();

    let warmupTierCounts, warmupCompileMs, warmupPhaseMs, warmupOsrExits;
    let perIterMs = null;

    while (warmupIterations < WARMUP_BUDGET) {
        ${isAsync ? "await " : ""}oneIteration();
        warmupIterations++;

        const cur = dfgFtlCompileMs();
        if (cur !== prevCompileMs) {
            consecutiveFrozenIterations = 0;
            prevCompileMs = cur;
            continue;
        }
        if (++consecutiveFrozenIterations < FREEZE_ITERATIONS_REQUIRED)
            continue;

        verificationsTried++;
        const snapshotTierCounts = $vm.codeBlockTierCounts();
        const snapshotCompileMs  = $vm.compileTimeTotals();
        const snapshotPhaseMs    = $vm.phaseTimeTotals();
        const snapshotOsrExits   = $vm.osrExitCounts();
        const tierCountsBefore   = ftlDfgCount();
        $vm.resetJITStats();

        const candidatePerIterMs = [];
        for (let v = 0; v < MEASUREMENT_ITERS; v++) {
            const start = performance.now();
            ${isAsync ? "await " : ""}oneIteration();
            candidatePerIterMs.push(Math.max(1, performance.now() - start));
        }
        warmupIterations += MEASUREMENT_ITERS;

        const windowCompile = $vm.compileTimeTotals();
        const recompiled = (windowCompile.dfgMs + windowCompile.ftlMs) !== 0;
        const retiered = ftlDfgCount() !== tierCountsBefore;
        if (!recompiled && !retiered) {
            reachedFrozenState = true;
            warmupTierCounts = snapshotTierCounts;
            warmupCompileMs  = snapshotCompileMs;
            warmupPhaseMs    = snapshotPhaseMs;
            warmupOsrExits   = snapshotOsrExits;
            perIterMs        = candidatePerIterMs;
            break;
        }

        consecutiveFrozenIterations = 0;
        prevCompileMs = dfgFtlCompileMs();
    }

    if (__benchmark.validate)
        __benchmark.validate();

    if (!reachedFrozenState) {
        print("__FTL_ONLY_REPORT__" + JSON.stringify({
            test: ${JSON.stringify(TEST_NAME)},
            skipped: false,
            reachedFrozenState: false,
            warmupIterations,
            verificationsTried,
        }));
        top.currentResolve([]);
        return;
    }

    // Deltas over the measurement window: zero compile/tier change by construction,
    // but a nonzero OSR-exit count surfaces a steady-state exit storm.
    const measurementOsrExits = $vm.osrExitCounts();
    const measurementCompileMs = $vm.compileTimeTotals();
    const measurementTierCounts = $vm.codeBlockTierCounts();

    print("__FTL_ONLY_REPORT__" + JSON.stringify({
        test: ${JSON.stringify(TEST_NAME)},
        skipped: false,
        reachedFrozenState: true,
        warmupIterations,
        verificationsTried,
        frozen: {
            tierCounts: warmupTierCounts,
            compileTimeMs: warmupCompileMs,
            phaseTimeMs: warmupPhaseMs,
            osrExits: warmupOsrExits,
        },
        measurement: {
            iterations: MEASUREMENT_ITERS,
            perIterMs,
            osrExitsDelta: measurementOsrExits,
            compileTimeMsDelta: measurementCompileMs,
            tierCounts: measurementTierCounts,
        },
    }));
    top.currentResolve(perIterMs);
${isAsync ? "})().catch((error) => { top.currentReject(error); });" : "})();"}
`;

Object.defineProperty(benchmark, "runnerCode", { value: runnerCode, configurable: true });

(async function () {
    try {
        await benchmark.fetchResources();
        await benchmark.run();
    } catch (e) {
        print("__FTL_ONLY_ERROR__" + JSON.stringify({
            test: TEST_NAME,
            error: String(e),
            stack: (e && e.stack) || null,
        }));
        quit();
    }
})();
