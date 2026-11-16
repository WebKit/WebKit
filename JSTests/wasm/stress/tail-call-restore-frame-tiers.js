//@ skip unless $isOMGPlatform
//@ slow!
//@ runDefaultWasm("--useDollarVM=1", "--useWasmTailCalls=true", "--useConcurrentJIT=0", "--wasmInliningMaximumWasmCalleeSize=0", "--thresholdForBBQOptimizeAfterWarmUp=0", "--thresholdForBBQOptimizeSoon=0", "--thresholdForOMGOptimizeAfterWarmUp=10", "--thresholdForOMGOptimizeSoon=10", "--bbqAllowlist=tail-call-restore-frame-tiers.bbq-allowlist", "--omgAllowlist=tail-call-restore-frame-tiers.omg-allowlist")

// Calls and tail calls between every pair of tiers (IPInt, BBQ, OMG, and OMG entered by loop OSR),
// within an instance and across instances, entered straight from JS and from deep in a wasm call
// chain, plus calls and tail calls to JS. Tail calls cover callee stack argument areas from 16 slots
// smaller to 16 slots larger than the caller's. A function's tier is pinned by its function index
// (index % 4) through the allowlists above, and every function asserts its tier with $vm before the
// call under test. After each call, the wasm frame that resumes checks its instance, its memory,
// and values that stay live across the call.

const IPInt = 0, BBQ = 1, OMG = 2, OSR = 3;
const tierNames = ["IPInt", "BBQ", "OMG", "OSR"];
const maxFunctionIndex = 199;

const I32 = 0x7f, I64 = 0x7e, F64 = 0x7c, FuncRef = 0x70, EmptyBlock = 0x40;
const op = {
    unreachable: 0x00, block: 0x02, loop: 0x03, if: 0x04, end: 0x0b, br: 0x0c, br_if: 0x0d, return: 0x0f,
    call: 0x10, call_indirect: 0x11, return_call: 0x12,
    local_get: 0x20, local_set: 0x21, local_tee: 0x22, global_get: 0x23,
    i32_load: 0x28, i64_load: 0x29, f64_load: 0x2b, memory_size: 0x3f,
    i32_const: 0x41, i64_const: 0x42, f64_const: 0x44,
    i32_eqz: 0x45, i32_ne: 0x47, i32_ge_u: 0x4f, i64_ne: 0x52, f64_ne: 0x62,
    i32_add: 0x6a, i32_sub: 0x6b, i32_mul: 0x6c, i64_add: 0x7c, i64_mul: 0x7e,
    f64_add: 0xa0, i64_extend_i32_u: 0xad, i64_reinterpret_f64: 0xbd,
};

function uleb(n) {
    let out = [];
    do {
        let byte = n & 0x7f;
        n >>>= 7;
        if (n)
            byte |= 0x80;
        out.push(byte);
    } while (n);
    return out;
}

function sleb(value) {
    value = BigInt(value);
    let out = [];
    while (true) {
        let byte = Number(value & 0x7fn);
        value >>= 7n;
        if ((value === 0n && !(byte & 0x40)) || (value === -1n && (byte & 0x40))) {
            out.push(byte);
            return out;
        }
        out.push(byte | 0x80);
    }
}

function f64Bytes(value) {
    return Array.from(new Uint8Array(new Float64Array([value]).buffer));
}

function name(string) {
    return [...uleb(string.length), ...Array.from(string, c => c.charCodeAt(0))];
}

function vec(items) {
    return [...uleb(items.length), ...items.flat()];
}

function section(id, items) {
    let body = vec(items);
    return [id, ...uleb(body.length), ...body];
}

const f64View = new Float64Array(1);
const i64View = new BigInt64Array(f64View.buffer);
function bitsOf(type, value) {
    if (type === I64)
        return BigInt.asIntN(64, value);
    f64View[0] = value;
    return i64View[0];
}

// 8 i64 and 8 f64 register arguments, then stackSlots alternating i64/f64 arguments.
function paramTypes(stackSlots) {
    let types = [];
    for (let i = 0; i < 8; ++i)
        types.push(I64);
    for (let i = 0; i < 8; ++i)
        types.push(F64);
    for (let i = 0; i < stackSlots; ++i)
        types.push(i % 2 ? F64 : I64);
    return types;
}

const callerStackSlots = 16;
const callerTypes = paramTypes(callerStackSlots);
const callerArgs = callerTypes.map((type, i) => type === I64 ? BigInt.asIntN(64, 0x0123456789abn * BigInt(i + 1) + 0x1111n) : (i + 1) * 1.25);

function checksum(types, values) {
    let sum = 0n;
    for (let i = 0; i < types.length; ++i)
        sum = BigInt.asIntN(64, sum + bitsOf(types[i], values[i]) * BigInt(i + 1));
    return sum;
}

// Each callee argument comes from a caller argument of the same type plus a constant, so the
// callee's arguments are moved out of the caller's argument area while it is being overwritten.
function calleeArgumentPlan(calleeTypes) {
    let callerIndicesOfType = type => callerTypes.map((t, i) => t === type ? i : -1).filter(i => i >= 0);
    return calleeTypes.map((type, k) => {
        let candidates = callerIndicesOfType(type);
        let source = candidates[(k * 7 + 3) % candidates.length];
        let addend = type === I64 ? BigInt(k * 0x10101 + 7) : k + 0.25;
        return { type, source, addend };
    });
}

function calleeArgs(plan) {
    return plan.map(({ type, source, addend }) => type === I64 ? BigInt.asIntN(64, callerArgs[source] + addend) : callerArgs[source] + addend);
}

function emitCalleeArgs(plan) {
    let code = [];
    for (let { type, source, addend } of plan) {
        code.push(op.local_get, ...uleb(source));
        if (type === I64)
            code.push(op.i64_const, ...sleb(addend), op.i64_add);
        else
            code.push(op.f64_const, ...f64Bytes(addend), op.f64_add);
    }
    return code;
}

function emitChecksumOfParams(types) {
    let code = [op.i64_const, 0];
    types.forEach((type, i) => {
        code.push(op.local_get, ...uleb(i));
        if (type === F64)
            code.push(op.i64_reinterpret_f64);
        code.push(op.i64_const, ...sleb(i + 1), op.i64_mul, op.i64_add);
    });
    return code;
}

const instanceMemorySize = { A: 1, B: 2 };
const instanceId = { A: 0xA, B: 0xB };
const liveI64Count = 8, liveF64Count = 4;

// Build one module. kind is "call" or "tail"; delta is the callee's extra stack argument slots.
function buildModule(kind, delta) {
    let calleeTypes = paramTypes(callerStackSlots + delta);
    let plan = calleeArgumentPlan(calleeTypes);

    let types = [];
    let typeIndex = (params, results) => {
        let key = JSON.stringify([params, results]);
        let index = types.findIndex(t => t.key === key);
        if (index < 0) {
            types.push({ key, bytes: [0x60, ...vec(params.map(p => [p])), ...vec(results.map(r => [r]))] });
            index = types.length - 1;
        }
        return index;
    };
    let probeType = typeIndex([], [I32]);
    let callerType = typeIndex(callerTypes, [I64]);
    let calleeType = typeIndex(calleeTypes, [I64]);
    let deepType = typeIndex([I32], [I64]);
    let fillerType = typeIndex([], []);

    let cases = [];
    for (let context of ["js", "deep"]) {
        for (let callerTier of [IPInt, BBQ, OMG, OSR]) {
            for (let calleeTier of [IPInt, BBQ, OMG, OSR]) {
                for (let target of ["same", "cross"])
                    cases.push({ kind, delta, context, callerTier, calleeTier, target });
            }
            cases.push({ kind, delta, context, callerTier, calleeTier: null, target: "js" });
        }
    }

    // Function imports: the four tier probes, the JS callee, then one cross-instance callee per case.
    let imports = [];
    for (let probe of ["ipint", "bbq", "omg", "osr"])
        imports.push([...name("vm"), ...name(probe), 0x00, ...uleb(probeType)]);
    imports.push([...name("env"), ...name("js"), 0x00, ...uleb(calleeType)]);
    const jsCalleeFunction = 4;
    for (let c of cases) {
        if (c.target === "cross") {
            c.crossImport = imports.length;
            imports.push([...name("cross"), ...name("g" + cases.indexOf(c)), 0x00, ...uleb(calleeType)]);
        }
    }
    let functionImportCount = imports.length;
    imports.push([...name("env"), ...name("mem"), 0x02, 0x00, ...uleb(1)]);
    imports.push([...name("env"), ...name("id"), 0x03, I32, 0x00]);
    const idGlobal = 0, armedGlobal = 1;

    let slots = [];
    let allocate = tier => {
        let index = tier;
        while (slots[index])
            index += 4;
        if (index > maxFunctionIndex)
            throw new Error("function index " + index + " is past the allowlists");
        slots[index] = { tier };
        return index;
    };
    let functionIndex = codeIndex => functionImportCount + codeIndex;

    for (let c of cases) {
        c.caller = allocate(c.callerTier);
        if (c.calleeTier !== null)
            c.callee = allocate(c.calleeTier);
    }
    let deepChain = [allocate(IPInt), allocate(BBQ), allocate(OMG)];
    let tableEntries = cases.filter(c => c.context === "deep").map(c => functionIndex(c.caller));
    cases.filter(c => c.context === "deep").forEach((c, i) => c.tableIndex = i);

    let probeCall = tier => [op.call, ...uleb(tier)];
    let returnIfNotArmed = tier => [op.global_get, armedGlobal, op.i32_eqz, op.if, EmptyBlock, ...probeCall(tier), op.i64_extend_i32_u, op.return, op.end];
    // Runs the loop until the frame has been OSR entered into OMG.
    let waitForTier = (tier, counterLocal) => {
        if (tier !== OSR)
            return [...probeCall(tier), op.i32_eqz, op.if, EmptyBlock, op.unreachable, op.end];
        return [
            op.block, EmptyBlock, op.loop, EmptyBlock,
            ...probeCall(OSR), op.br_if, 1,
            op.local_get, ...uleb(counterLocal), op.i32_const, ...sleb(1), op.i32_add, op.local_tee, ...uleb(counterLocal),
            op.i32_const, ...sleb(10000000), op.i32_ge_u, op.if, EmptyBlock, op.unreachable, op.end,
            op.br, 0, op.end, op.end,
        ];
    };

    // Saves the instance id, the last word of memory, and live values loaded from memory, then checks them after the call.
    let liveState = firstLocal => {
        let idLocal = firstLocal, addressLocal = firstLocal + 1, lastWordLocal = firstLocal + 2;
        let i64Local = k => firstLocal + 3 + k;
        let f64Local = k => firstLocal + 3 + liveI64Count + k;
        let save = [
            op.global_get, idGlobal, op.local_set, ...uleb(idLocal),
            op.memory_size, 0, op.i32_const, ...sleb(65536), op.i32_mul, op.i32_const, ...sleb(4), op.i32_sub, op.local_tee, ...uleb(addressLocal),
            op.i32_load, 2, 0, op.local_set, ...uleb(lastWordLocal),
        ];
        let check = [
            op.global_get, idGlobal, op.local_get, ...uleb(idLocal), op.i32_ne, op.if, EmptyBlock, op.unreachable, op.end,
            op.local_get, ...uleb(addressLocal), op.i32_load, 2, 0, op.local_get, ...uleb(lastWordLocal), op.i32_ne, op.if, EmptyBlock, op.unreachable, op.end,
        ];
        for (let k = 0; k < liveI64Count; ++k) {
            save.push(op.i32_const, 0, op.i64_load, 3, ...uleb(k * 8), op.local_set, ...uleb(i64Local(k)));
            check.push(op.i32_const, 0, op.i64_load, 3, ...uleb(k * 8), op.local_get, ...uleb(i64Local(k)), op.i64_ne, op.if, EmptyBlock, op.unreachable, op.end);
        }
        for (let k = 0; k < liveF64Count; ++k) {
            save.push(op.i32_const, 0, op.f64_load, 3, ...uleb(64 + k * 8), op.local_set, ...uleb(f64Local(k)));
            check.push(op.i32_const, 0, op.f64_load, 3, ...uleb(64 + k * 8), op.local_get, ...uleb(f64Local(k)), op.f64_ne, op.if, EmptyBlock, op.unreachable, op.end);
        }
        let locals = [[3, I32], [liveI64Count, I64], [liveF64Count, F64]];
        return { save, check, locals };
    };

    let bodies = [];
    let functionTypes = [];
    let define = (codeIndex, type, locals, code) => {
        functionTypes[codeIndex] = type;
        let body = [...vec(locals.map(([count, t]) => [...uleb(count), t])), ...code, op.end];
        bodies[codeIndex] = [...uleb(body.length), ...body];
    };

    let callerParamsChecksum = checksum(callerTypes, callerArgs);
    for (let c of cases) {
        let counterLocal = callerTypes.length;
        let state = liveState(callerTypes.length + 1);
        let target = c.target === "js" ? jsCalleeFunction : c.target === "cross" ? c.crossImport : functionIndex(c.callee);
        let code = [...returnIfNotArmed(c.callerTier), ...waitForTier(c.callerTier, counterLocal)];
        if (kind === "tail")
            code.push(...emitCalleeArgs(plan), op.return_call, ...uleb(target));
        else {
            code.push(...state.save, ...emitCalleeArgs(plan), op.call, ...uleb(target), ...state.check);
            code.push(...emitChecksumOfParams(callerTypes), op.i64_const, ...sleb(callerParamsChecksum), op.i64_ne, op.if, EmptyBlock, op.unreachable, op.end);
        }
        define(c.caller, callerType, [[1, I32], ...state.locals], code);

        if (c.calleeTier !== null) {
            let calleeCode = [...returnIfNotArmed(c.calleeTier), ...waitForTier(c.calleeTier, calleeTypes.length), ...emitChecksumOfParams(calleeTypes)];
            define(c.callee, calleeType, [[1, I32]], calleeCode);
        }
        c.expected = checksum(calleeTypes, calleeArgs(plan));
    }

    deepChain.forEach((codeIndex, depth) => {
        let tier = [IPInt, BBQ, OMG][depth];
        let state = liveState(1);
        let code = [...returnIfNotArmed(tier), ...waitForTier(tier, 1), ...state.save, op.local_get, 0];
        if (depth < deepChain.length - 1)
            code.push(op.call, ...uleb(functionIndex(deepChain[depth + 1])));
        else {
            code = [...returnIfNotArmed(tier), ...waitForTier(tier, 1), ...state.save];
            callerTypes.forEach((type, i) => {
                if (type === I64)
                    code.push(op.i64_const, ...sleb(callerArgs[i]));
                else
                    code.push(op.f64_const, ...f64Bytes(callerArgs[i]));
            });
            code.push(op.local_get, 0, op.call_indirect, ...uleb(callerType), 0);
        }
        code.push(...state.check);
        define(codeIndex, deepType, state.locals, code);
    });

    for (let i = 0; i < slots.length; ++i) {
        if (!bodies[i])
            define(i, fillerType, [], []);
    }

    let exports = [[...name("armed"), 0x03, armedGlobal]];
    deepChain.forEach((codeIndex, depth) => exports.push([...name("deep" + depth), 0x00, ...uleb(functionIndex(codeIndex))]));
    cases.forEach((c, i) => {
        exports.push([...name("f" + i), 0x00, ...uleb(functionIndex(c.caller))]);
        if (c.calleeTier !== null)
            exports.push([...name("g" + i), 0x00, ...uleb(functionIndex(c.callee))]);
    });

    let bytes = [
        0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00,
        ...section(1, types.map(t => t.bytes)),
        ...section(2, imports),
        ...section(3, functionTypes.map(t => uleb(t))),
        ...section(4, [[FuncRef, 0x00, ...uleb(tableEntries.length)]]),
        ...section(6, [[I32, 0x01, op.i32_const, 0, op.end]]),
        ...section(7, exports),
        ...section(9, [[0x00, op.i32_const, 0, op.end, ...vec(tableEntries.map(f => uleb(f)))]]),
        ...section(10, bodies),
    ];
    return { module: new WebAssembly.Module(new Uint8Array(bytes)), cases, calleeTypes, slots };
}

function instantiate(module, cases, which, crossExports) {
    let memory = new WebAssembly.Memory({ initial: instanceMemorySize[which] });
    let words = new BigInt64Array(memory.buffer, 0, liveI64Count + liveF64Count);
    for (let k = 0; k < liveI64Count; ++k)
        words[k] = BigInt.asIntN(64, BigInt(instanceId[which]) * 0x100000001n + BigInt(k));
    let doubles = new Float64Array(memory.buffer, liveI64Count * 8, liveF64Count);
    for (let k = 0; k < liveF64Count; ++k)
        doubles[k] = instanceId[which] * 100 + k + 0.5;
    new Int32Array(memory.buffer)[memory.buffer.byteLength / 4 - 1] = instanceId[which];

    let cross = {};
    cases.forEach((c, i) => {
        if (c.target === "cross")
            cross["g" + i] = crossExports ? crossExports["g" + i] : () => { throw new Error("instance B's callers are never called"); };
    });
    return new WebAssembly.Instance(module, {
        vm: { ipint: $vm.isIPIntFrame, bbq: $vm.isBBQFrame, omg: $vm.isOMGFrame, osr: $vm.isOMGForOSREntryFrame },
        env: { mem: memory, id: instanceId[which], js: jsCallee },
        cross,
    });
}

let jsCalleeTypes;
function jsCallee(...args) {
    if (args.length !== jsCalleeTypes.length)
        throw new Error("bad argument count " + args.length);
    return checksum(jsCalleeTypes, args);
}

function warmUp(exported, args, description) {
    for (let i = 0; i < 100000; ++i) {
        if (exported(...args) === 1n)
            return;
    }
    throw new Error(description + " never reached its tier");
}

let covered = new Set();
function run(kind, delta) {
    let { module, cases, calleeTypes } = buildModule(kind, delta);
    jsCalleeTypes = calleeTypes;
    let calleeArgValues = calleeArgs(calleeArgumentPlan(calleeTypes));
    let B = instantiate(module, cases, "B", null);
    let A = instantiate(module, cases, "A", B.exports);

    cases.forEach((c, i) => {
        if (c.calleeTier !== null && c.calleeTier !== OSR)
            warmUp((c.target === "cross" ? B : A).exports["g" + i], calleeArgValues, "callee of case " + i);
        if (c.callerTier !== OSR)
            warmUp(A.exports["f" + i], callerArgs, "caller of case " + i);
    });
    for (let depth = 0; depth < 3; ++depth)
        warmUp(A.exports["deep" + depth], [0], "deep chain function " + depth);

    A.exports.armed.value = 1;
    B.exports.armed.value = 1;
    cases.forEach((c, i) => {
        let description = `${c.kind} ${tierNames[c.callerTier]} -> ${c.calleeTier === null ? "JS" : tierNames[c.calleeTier]} ${c.target} from ${c.context}, ${c.delta} stack slots`;
        let result = c.context === "js" ? A.exports["f" + i](...callerArgs) : A.exports.deep0(c.tableIndex);
        if (result !== c.expected)
            throw new Error(description + ": got " + result + ", expected " + c.expected);
        covered.add(JSON.stringify([c.kind, c.callerTier, c.calleeTier, c.target, c.context, c.delta]));
    });
}

run("call", 0);
for (let delta = -16; delta <= 16; ++delta)
    run("tail", delta);

let expectedCount = 0;
for (let kind of ["call", "tail"]) {
    for (let delta of kind === "call" ? [0] : Array.from({ length: 33 }, (_, i) => i - 16)) {
        for (let context of ["js", "deep"]) {
            for (let callerTier of [IPInt, BBQ, OMG, OSR]) {
                for (let calleeTier of [IPInt, BBQ, OMG, OSR]) {
                    for (let target of ["same", "cross"]) {
                        if (!covered.has(JSON.stringify([kind, callerTier, calleeTier, target, context, delta])))
                            throw new Error("missing " + [kind, callerTier, calleeTier, target, context, delta]);
                        ++expectedCount;
                    }
                }
                if (!covered.has(JSON.stringify([kind, callerTier, null, "js", context, delta])))
                    throw new Error("missing " + [kind, callerTier, "js", context, delta]);
                ++expectedCount;
            }
        }
    }
}
if (covered.size !== expectedCount)
    throw new Error("covered " + covered.size + " cases, expected " + expectedCount);
