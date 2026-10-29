//@ runDefaultWasm("--thresholdForBBQOptimizeAfterWarmUp=150", "--thresholdForBBQOptimizeSoon=50", "--thresholdForOMGOptimizeAfterWarmUp=50000")
//
// The thresholds are pinned to their shipping defaults because the warm-up counts below are
// sized against them: if a default moved, a tier-up would land in the wrong phase of the
// schedule and the test would pass without having exercised anything.

// Two VMs share one WebAssembly.Module, and therefore one CalleeGroup. The main VM sits in
// nested IPInt A frames and drives the whole schedule from inside the innermost import callback;
// the worker only parks the victim frame.
//
//   1. B and then A tier up to BBQ. A_BBQ1's direct branch targets B_BBQ1.
//   2. The worker enters A_BBQ1 and parks in an import call, just before the branch to B.
//   3. A tiers up to OMG, retiring A_BBQ1 into the single weak BBQ slot.
//   4. The main VM returns from the innermost import callback. Its nested IPInt A frames unwind,
//      and their epilogue OSR checks publish A_BBQ2 over the weak A_BBQ1 slot.
//   5. B tiers up to OMG, which must repatch every live caller of B, including A_BBQ1.
//   6. The worker is released and A_BBQ1 takes its direct branch, reporting which B it reached.

function encodeULEB(value) {
    value = BigInt(value);
    const result = [];
    do {
        let byte = Number(value & 0x7fn);
        value >>= 7n;
        if (value)
            byte |= 0x80;
        result.push(byte);
    } while (value);
    return result;
}

function section(id, payload) {
    return [id, ...encodeULEB(payload.length), ...payload];
}

function functionBody(instructions) {
    const body = [0, ...instructions]; // zero local declarations
    return [...encodeULEB(body.length), ...body];
}

// (module
//   (import "m" "g" (func $g (param i32)))            ;; parks the victim / drives the schedule
//   (import "m" "t" (func $t (result i32)))           ;; $vm.omgTrue: reports the caller's tier
//   (func $A (export "A") (param i32 i32) (result i32)
//     (call $g (local.get 0))
//     (block (loop (br_if 1 (i32.eqz (local.get 1)))
//                  (local.set 1 (i32.sub (local.get 1) (i32.const 1)))
//                  (br 0)))
//     (if (result i32) (local.get 0) (then (call $B)) (else (i32.const -1))))
//   (func $B (export "B") (result i32) (call $t)))
function makeModuleBytes() {
    const typeSection = section(1, [
        3,
        0x60, 1, 0x7f, 0,
        0x60, 0, 1, 0x7f,
        0x60, 2, 0x7f, 0x7f, 1, 0x7f,
    ]);
    const importSection = section(2, [
        2,
        1, 0x6d, 1, 0x67, 0, 0,
        1, 0x6d, 1, 0x74, 0, 1,
    ]);
    const functionSection = section(3, [2, 2, 1]);
    const exportSection = section(7, [2, 1, 0x41, 0, 2, 1, 0x42, 0, 3]);
    const aBody = functionBody([
        0x20, 0, 0x10, 0,
        0x02, 0x40, 0x03, 0x40,
        0x20, 1, 0x45, 0x0d, 1,
        0x20, 1, 0x41, 1, 0x6b, 0x21, 1, 0x0c, 0,
        0x0b, 0x0b,
        0x20, 0, 0x04, 0x7f,
        0x10, 3,
        0x05, 0x41, 0x7f,
        0x0b, 0x0b,
    ]);
    const bBody = functionBody([0x10, 1, 0x0b]);
    const codeSection = section(10, [2, ...aBody, ...bBody]);
    return new Uint8Array([0, 0x61, 0x73, 0x6d, 1, 0, 0, 0,
        ...typeSection, ...importSection, ...functionSection, ...exportSection, ...codeSection]);
}

// What B returns, i.e. which compiled version of B its caller reached.
const REACHED_BBQ = 0;
const REACHED_OMG = 1;

const IPINT_FRAMES = 64;
// Cap on how many driving calls a single tier-up wait will make before giving up.
const MAX_DRIVE_CALLS = 5000000;
// A_BBQ2's publication is the one step with nothing to poll for: a fixed engine declines that
// install, so its absence is the expected outcome there and cannot be waited on. Give the
// re-tier compile this long to land before moving on.
const RETIER_GRACE_MS = 200;

const BBQ = 0, OMG = 1, BBQ_RETIRED = 2;

function calleeState(exportedFunction) {
    return $vm.wasmCalleeIDs(exportedFunction);
}

// Keep calling drive() until the callee group reaches the state we need. This makes each wait
// take exactly as long as the concurrent compile actually takes.
function driveUntil(exportedFunction, predicate, drive, label) {
    for (let n = 0; n < MAX_DRIVE_CALLS; ++n) {
        if (predicate(calleeState(exportedFunction)))
            return;
        drive();
    }
    throw new Error(`gave up waiting for ${label}`);
}

const I = Object.freeze({
    victimReady: 0,
    startVictim: 1,
    victimBlocked: 2,
    releaseVictim: 3,
    victimDone: 4,
    victimResult: 5,
    agentError: 6,
});

const controlSAB = new SharedArrayBuffer(8 * 4);
const control = new Int32Array(controlSAB);

function wake(index) {
    Atomics.notify(control, index, Infinity);
}

function waitFor(index, expected, label, timeoutMS = 60000) {
    const deadline = Date.now() + timeoutMS;
    while (Atomics.load(control, index) !== expected) {
        if (Atomics.load(control, I.agentError))
            throw new Error(`agent failed while waiting for ${label}`);
        if (Date.now() >= deadline)
            throw new Error(`timeout waiting for ${label}; got ${Atomics.load(control, index)}, expected ${expected}`);
        Atomics.wait(control, index, Atomics.load(control, index), 50);
    }
}

const victimWorker = String.raw`
let module, control;
$262.agent.receiveBroadcast((value) => { module = value; });
$262.agent.receiveBroadcast((value) => { control = new Int32Array(value); });
const I = { victimReady:0, startVictim:1, victimBlocked:2, releaseVictim:3,
    victimDone:4, victimResult:5, agentError:6 };
try {
    const instance = new WebAssembly.Instance(module, { m: { t: $vm.omgTrue, g() {
        Atomics.store(control, I.victimBlocked, 1);
        Atomics.notify(control, I.victimBlocked, Infinity);
        while (!Atomics.load(control, I.releaseVictim))
            Atomics.wait(control, I.releaseVictim, 0, 100);
    } } });

    Atomics.store(control, I.victimReady, 1);
    Atomics.notify(control, I.victimReady, Infinity);
    while (!Atomics.load(control, I.startVictim))
        Atomics.wait(control, I.startVictim, 0, 100);

    // Enters A_BBQ1 and blocks immediately before A's direct call to B. On release it takes that
    // branch, and B reports which of its compiled versions was reached.
    const result = instance.exports.A(1, 0);
    Atomics.store(control, I.victimResult, result);
    Atomics.store(control, I.victimDone, 1);
    Atomics.notify(control, I.victimDone, Infinity);
} catch (error) {
    Atomics.store(control, I.agentError, 1);
    Atomics.notify(control, I.agentError, Infinity);
    $262.agent.report('victim-error:' + error + '\n' + error.stack);
}
`;

$262.agent.start(victimWorker);

const sharedModule = new WebAssembly.Module(makeModuleBytes());
$262.agent.broadcast(sharedModule, 1);
$262.agent.broadcast(controlSAB, 2);

// Everything here runs with IPINT_FRAMES + 1 IPInt frames of A live underneath us. Returning
// unwinds them, and their epilogues are what publish A_BBQ2.
function runScheduleUnderIPIntFrames() {
    waitFor(I.victimReady, 1, 'victim ready');

    // Put B in BBQ. A_BBQ1 has to be compiled while B_BBQ1 is its direct-call target.
    driveUntil(manager.exports.B, state => state[BBQ], () => manager.exports.B(), 'B_BBQ1');

    // Compile A_BBQ1.
    driveUntil(manager.exports.A, state => state[BBQ], () => manager.exports.A(0, 0), 'A_BBQ1');
    bbq1ID = calleeState(manager.exports.A)[BBQ];

    // Park a real A_BBQ1 frame immediately before its call to B.
    Atomics.store(control, I.startVictim, 1);
    wake(I.startVictim);
    waitFor(I.victimBlocked, 1, 'A_BBQ1 frame parked');

    // Publish A_OMG and wait for releaseBBQCallee() to retire A_BBQ1, which is what re-permits
    // BBQ compilation of A. These calls run in A_BBQ1/A_OMG rather than IPInt, so the only IPInt
    // epilogues left for A are the ones belonging to the frames below us.
    driveUntil(manager.exports.A, state => state[OMG] && state[BBQ_RETIRED],
        () => manager.exports.A(0, 0), 'A_OMG with A_BBQ1 retired');
}

let remainingIPIntFrames = IPINT_FRAMES;
let ranSchedule = false;
let bbq1ID = 0;
const manager = new WebAssembly.Instance(sharedModule, { m: { t: $vm.omgTrue, g() {
    if (remainingIPIntFrames) {
        // Nest before anything has tiered up, so all of these stay IPInt frames.
        --remainingIPIntFrames;
        manager.exports.A(0, 0);
        return;
    }
    if (ranSchedule)
        return;
    ranSchedule = true;
    runScheduleUnderIPIntFrames();
} } });

// Enter A before anything has tiered up. This nests IPINT_FRAMES deep, runs the schedule above
// at the bottom, then unwinds -- and the unwinding epilogues cross the threshold that
// releaseBBQCallee() reset, publishing A_BBQ2 over the weak A_BBQ1 slot.
manager.exports.A(0, 0);

// Give the re-tier BBQ compile time to publish A_BBQ2 over the weak A_BBQ1 slot. A fixed engine
// declines that install, so stop early if it lands and otherwise just move on.
const retierDeadline = Date.now() + RETIER_GRACE_MS;
while (calleeState(manager.exports.A)[BBQ] === bbq1ID && Date.now() < retierDeadline)
    $262.agent.sleep(1);

// Publish B_OMG. This has to repatch A_BBQ1's direct call as well as A_BBQ2's and A_OMG's.
driveUntil(manager.exports.B, state => state[OMG], () => manager.exports.B(), 'B_OMG');

// Return from the import and let A_BBQ1 take its direct call.
Atomics.store(control, I.releaseVictim, 1);
wake(I.releaseVictim);
waitFor(I.victimDone, 1, 'victim returned');

const victimResult = Atomics.load(control, I.victimResult);
if (victimResult !== REACHED_OMG) {
    throw new Error(victimResult === REACHED_BBQ
        ? 'A_BBQ1 still calls the retired B_BBQ1: its direct callsite was never repatched'
        : `unexpected result from the victim: ${victimResult}`);
}
