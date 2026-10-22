//@ skip if $architecture != "arm64" && $architecture != "x86_64"
//@ runDefaultWasm("--useWasmIPInt=1", "--useBBQJIT=0", "--useOMGJIT=0")

// Every function is run both inside a module with all the other functions of its group and alone in
// a single-function module. In the single-function module the code section is the last section, so
// the immediates at the end of the body are within a few bytes of the end of the module. IPInt reads a
// multi-byte immediate with one 8-byte load only when those bytes lie inside the function, so each group
// also runs with nops after every body.

const iterations = 1000;

const I32 = 0x7f;
const I64 = 0x7e;
const F32 = 0x7d;
const F64 = 0x7c;

const Op = {
    nop: 0x01,
    end: 0x0b,
    localGet: 0x20,
    localSet: 0x21,
    localTee: 0x22,
    i32Load: 0x28,
    i64Load: 0x29,
    i32Load8U: 0x2d,
    i64Load32S: 0x34,
    i32Store: 0x36,
    i64Store: 0x37,
    i32Store16: 0x3b,
    i32Const: 0x41,
    i64Const: 0x42,
    i32Add: 0x6a,
    i64Add: 0x7c,
    prefixGC: 0xfb,
    prefixFC: 0xfc,
    prefixSIMD: 0xfd,
    prefixAtomic: 0xfe,
};

const GCOp = {
    refI31: 0x1c,
    i31GetS: 0x1d,
    i31GetU: 0x1e,
};

const FCOp = {
    i32TruncSatF32S: 0,
    i32TruncSatF32U: 1,
    i64TruncSatF64S: 6,
};

const AtomicOp = {
    fence: 0x03,
    i32AtomicLoad: 0x10,
    i64AtomicLoad: 0x11,
    i32AtomicStore: 0x17,
    i32AtomicRmwAdd: 0x1e,
};

const SIMDOp = {
    v128Const: 0x0c,
    i32x4Splat: 0x11,
    i32x4ExtractLane: 0x1b,
    i32x4Add: 0xae,
};

const INT32_MAX = 0x7fffffff;
const INT32_MIN = -0x80000000;
const INT64_MAX = (1n << 63n) - 1n;
const INT64_MIN = -(1n << 63n);

function minimalSignedLEBLength(value) {
    let length = 1;
    for (value = BigInt(value); value >= 64n || value < -64n; value >>= 7n)
        ++length;
    return length;
}

function minimalUnsignedLEBLength(value) {
    let length = 1;
    for (value = BigInt(value); value >= 128n; value >>= 7n)
        ++length;
    return length;
}

// Writes exactly `length` bytes. Lengths above the minimal one produce padded encodings, whose extra
// bytes carry the sign extension (0x80 ... 0x00 for non-negative values, 0xff ... 0x7f for negative).
function encodeLEB(value, length) {
    const bytes = [];
    for (let i = 0; i < length; ++i) {
        let byte = Number(value & 0x7fn);
        value >>= 7n;
        if (i !== length - 1)
            byte |= 0x80;
        bytes.push(byte);
    }
    return bytes;
}

function sleb(value, length, maxLength) {
    if (length < minimalSignedLEBLength(value) || length > maxLength)
        throw new Error(`Bad signed LEB length ${length} for ${value}`);
    return encodeLEB(BigInt(value), length);
}

function uleb(value, length = minimalUnsignedLEBLength(value)) {
    if (length < minimalUnsignedLEBLength(value) || length > 5)
        throw new Error(`Bad unsigned LEB length ${length} for ${value}`);
    return encodeLEB(BigInt(value), length);
}

function uleb64(value, length = minimalUnsignedLEBLength(value)) {
    if (length < minimalUnsignedLEBLength(value) || length > 10)
        throw new Error(`Bad unsigned LEB length ${length} for ${value}`);
    return encodeLEB(BigInt(value), length);
}

function lengthsFrom(minimalLength, maxLength) {
    const lengths = [];
    for (let length = minimalLength; length <= maxLength; ++length)
        lengths.push(length);
    return lengths;
}

function hex(bytes) {
    return bytes.map(byte => byte.toString(16).padStart(2, "0")).join(" ");
}

function vec(items) {
    return [...uleb(items.length), ...items.flat()];
}

function name(string) {
    return vec([...string].map(c => [c.charCodeAt(0)]));
}

function section(id, contents) {
    return [id, ...uleb(contents.length), ...contents];
}

// Each function is { params, results, locals: [[count, type], ...], body } where body excludes the final `end`.
// Function i is exported as "f" + i. The code section is always the last section.
function buildModule(funcs, importedMemoryPages, memory64Pages) {
    const sections = [];
    sections.push(section(1, vec(funcs.map(f => [0x60, ...vec(f.params.map(t => [t])), ...vec(f.results.map(t => [t]))]))));
    if (importedMemoryPages !== undefined)
        sections.push(section(2, vec([[...name("env"), ...name("memory"), 0x02, 0x00, ...uleb(importedMemoryPages)]])));
    sections.push(section(3, vec(funcs.map((f, i) => uleb(i)))));
    if (memory64Pages !== undefined)
        sections.push(section(5, vec([[0x04, ...uleb(memory64Pages)]])));
    sections.push(section(7, vec(funcs.map((f, i) => [...name("f" + i), 0x00, ...uleb(i)]))));
    sections.push(section(10, vec(funcs.map(f => {
        const body = [...vec(f.locals.map(([count, type]) => [...uleb(count), type])), ...f.body, Op.end];
        return [...uleb(body.length), ...body];
    }))));
    return new Uint8Array([0x00, 0x61, 0x73, 0x6d, 0x01, 0x00, 0x00, 0x00, ...sections.flat()]);
}

function check(label, run, expected) {
    for (let i = 0; i < iterations; ++i) {
        const actual = run(i);
        const want = expected(i);
        if (actual !== want)
            throw new Error(`${label}: iteration ${i}: expected ${want} but got ${actual}`);
    }
}

function checkTraps(label, run) {
    for (let i = 0; i < iterations; ++i) {
        let trapped = false;
        try {
            run(i);
        } catch (e) {
            if (!(e instanceof WebAssembly.RuntimeError))
                throw new Error(`${label}: iteration ${i}: expected a RuntimeError but got ${e}`);
            trapped = true;
        }
        if (!trapped)
            throw new Error(`${label}: iteration ${i}: expected an out-of-bounds trap`);
    }
}

const memoryPages = 40;
const memory = new WebAssembly.Memory({ initial: memoryPages });
const view = new DataView(memory.buffer);

function runInstance(label, cases, importsMemory, memory64) {
    const bytes = buildModule(cases, importsMemory ? memoryPages : undefined, memory64 ? 1 : undefined);
    const instance = new WebAssembly.Instance(new WebAssembly.Module(bytes), { env: { memory } });
    cases.forEach((c, i) => c.test(instance.exports["f" + i], `${label}: ${c.name}`));
}

function runGroup(groupName, cases, { importsMemory = false, memory64 = false } = {}) {
    runInstance(`${groupName} (module with ${cases.length} functions)`, cases, importsMemory, memory64);
    for (const c of cases)
        runInstance(`${groupName} (single-function module)`, [c], importsMemory, memory64);
    const paddedCases = cases.map((c) => ({ ...c, body: [...c.body, ...new Array(8).fill(Op.nop)] }));
    runInstance(`${groupName} (module with ${cases.length} nop-padded functions)`, paddedCases, importsMemory, memory64);
}

// i32.const
{
    const values = [
        0, 1, -1, 63, -64,
        64, -65, 8191, -8192,
        8192, -8193, 2 ** 20 - 1, -(2 ** 20),
        2 ** 20, -(2 ** 20) - 1, 2 ** 27 - 1, -(2 ** 27),
        2 ** 27, -(2 ** 27) - 1, INT32_MAX, INT32_MIN,
    ];
    const cases = [];
    for (const value of values) {
        for (const length of lengthsFrom(minimalSignedLEBLength(value), 5)) {
            const immediate = sleb(value, length, 5);
            cases.push({
                name: `i32.const ${value} encoded as ${hex(immediate)}`,
                params: [], results: [I32], locals: [],
                body: [Op.i32Const, ...immediate],
                test(fn, label) { check(label, () => fn(), () => value); },
            });
        }
    }
    runGroup("i32.const", cases);
}

// i64.const
{
    const values = [0n, 1n, -1n, INT64_MAX, INT64_MIN];
    for (let length = 1n; length <= 9n; ++length) {
        const max = (1n << (7n * length - 1n)) - 1n;
        const min = -(1n << (7n * length - 1n));
        values.push(max, min, max + 1n, min - 1n);
    }
    const cases = [];
    for (const value of values) {
        for (const length of lengthsFrom(minimalSignedLEBLength(value), 10)) {
            const immediate = sleb(value, length, 10);
            cases.push({
                name: `i64.const ${value} encoded as ${hex(immediate)}`,
                params: [], results: [I64], locals: [],
                body: [Op.i64Const, ...immediate],
                test(fn, label) { check(label, () => fn(), () => value); },
            });
        }
    }
    runGroup("i64.const", cases);
}

// Constants followed by bytes that would end a negative number (bit 7 clear, bit 6 set), such as the
// const and add opcodes. Only the constant's own last byte may decide its sign.
{
    const cases = [];
    for (const value of [128, 8192, -129, 2 ** 20]) {
        for (const length of lengthsFrom(minimalSignedLEBLength(value), 5)) {
            const immediate = sleb(value, length, 5);
            cases.push({
                name: `i32.const ${value} encoded as ${hex(immediate)}, then i32.const -1 and i32.add`,
                params: [], results: [I32], locals: [],
                body: [Op.i32Const, ...immediate, Op.i32Const, 0x7f, Op.i32Add],
                test(fn, label) { check(label, () => fn(), () => value - 1); },
            });
        }
    }
    for (const value of [128n, -129n, 1n << 40n]) {
        for (const length of lengthsFrom(minimalSignedLEBLength(value), 10)) {
            const immediate = sleb(value, length, 10);
            cases.push({
                name: `i64.const ${value} encoded as ${hex(immediate)}, then i64.const -1 and i64.add`,
                params: [], results: [I64], locals: [],
                body: [Op.i64Const, ...immediate, Op.i64Const, 0x7f, Op.i64Add],
                test(fn, label) { check(label, () => fn(), () => value - 1n); },
            });
        }
    }
    runGroup("constants followed by sign-like bytes", cases);
}

// local.get / local.set / local.tee. Local 0 is the parameter. Each access to the target local goes through
// either the encoding under test or the minimal encoding, so a misdecoded index produces a wrong result.
// Indices past 16383 need 3 bytes; maxFunctionLocals keeps 4 and 5 byte indices reachable only by padding.
{
    const cases = [];
    for (const index of [0, 1, 127, 128, 16383, 16384]) {
        const minimal = uleb(index);
        for (const length of lengthsFrom(minimal.length, 5)) {
            const encoded = uleb(index, length);
            cases.push({
                name: `local index ${index} encoded as ${hex(encoded)}`,
                params: [I32], results: [I32],
                locals: [[index < 128 ? 127 : 16384, I32]],
                body: [
                    Op.localGet, 0,
                    Op.localSet, ...encoded,
                    Op.localGet, ...minimal,
                    Op.i32Const, 1,
                    Op.i32Add,
                    Op.localTee, ...encoded,
                    Op.localGet, ...encoded,
                    Op.i32Add,
                    Op.localSet, ...minimal,
                    Op.localGet, ...encoded,
                ],
                test(fn, label) {
                    check(label, i => fn(i * 7 - 3500), i => (2 * (i * 7 - 3500) + 2) | 0);
                },
            });
        }
    }
    runGroup("locals", cases);
}

// Memory loads and stores with padded alignment and offset immediates.
{
    const patternByte = address => Math.imul(address + 1, 0x9e3779b1) >>> 24;
    const loadOffsets = [0, 4, 127, 128, 70000, 2097160];
    for (const offset of loadOffsets) {
        for (let address = offset; address < offset + 64; ++address)
            view.setUint8(address, patternByte(address));
    }

    const loadOps = [
        { name: "i32.load", opcode: Op.i32Load, alignment: 2, result: I32, read: address => view.getInt32(address, true) },
        { name: "i64.load", opcode: Op.i64Load, alignment: 3, result: I64, read: address => view.getBigInt64(address, true) },
        { name: "i32.load8_u", opcode: Op.i32Load8U, alignment: 0, result: I32, read: address => view.getUint8(address) },
        { name: "i64.load32_s", opcode: Op.i64Load32S, alignment: 2, result: I64, read: address => BigInt(view.getInt32(address, true)) },
    ];

    const i32Value = i => Math.imul(i + 1, 0x9e3779b1) | 0;
    const i64Value = i => BigInt.asIntN(64, BigInt(i + 1) * 0x9e3779b97f4a7c15n);
    const storeOps = [
        { name: "i32.store", opcode: Op.i32Store, alignment: 2, valueType: I32, value: i32Value, expected: i32Value, read: address => view.getInt32(address, true) },
        { name: "i64.store", opcode: Op.i64Store, alignment: 3, valueType: I64, value: i64Value, expected: i64Value, read: address => view.getBigInt64(address, true) },
        { name: "i32.store16", opcode: Op.i32Store16, alignment: 1, valueType: I32, value: i32Value, expected: i => i32Value(i) & 0xffff, read: address => view.getUint16(address, true) },
    ];
    const storeOffsets = [0, 128, 70000, 2097160];

    const cases = [];
    for (const op of loadOps) {
        for (const offset of loadOffsets) {
            for (const length of lengthsFrom(minimalUnsignedLEBLength(offset), 5)) {
                const alignment = uleb(op.alignment, 6 - length);
                const encodedOffset = uleb(offset, length);
                cases.push({
                    name: `${op.name} alignment ${hex(alignment)} offset ${offset} encoded as ${hex(encodedOffset)}`,
                    params: [I32], results: [op.result], locals: [],
                    body: [Op.localGet, 0, op.opcode, ...alignment, ...encodedOffset],
                    test(fn, label) {
                        const address = i => 8 + (i & 7);
                        check(label, i => fn(address(i)), i => op.read(address(i) + offset));
                    },
                });
            }
        }
    }
    for (const op of storeOps) {
        for (const offset of storeOffsets) {
            for (const length of lengthsFrom(minimalUnsignedLEBLength(offset), 5)) {
                const alignment = uleb(op.alignment, 6 - length);
                const encodedOffset = uleb(offset, length);
                cases.push({
                    name: `${op.name} alignment ${hex(alignment)} offset ${offset} encoded as ${hex(encodedOffset)}`,
                    params: [I32, op.valueType], results: [], locals: [],
                    body: [Op.localGet, 0, Op.localGet, 1, op.opcode, ...alignment, ...encodedOffset],
                    test(fn, label) {
                        const address = i => 1024 + (i & 7);
                        check(label, i => {
                            fn(address(i), op.value(i));
                            return op.read(address(i) + offset);
                        }, op.expected);
                    },
                });
            }
        }
    }
    for (const offset of [2 ** 28, 0xffffffff]) {
        const encodedOffset = uleb(offset);
        cases.push({
            name: `out-of-bounds i32.load offset ${offset} encoded as ${hex(encodedOffset)}`,
            params: [I32], results: [I32], locals: [],
            body: [Op.localGet, 0, Op.i32Load, 2, ...encodedOffset],
            test(fn, label) { checkTraps(label, i => fn(i & 7)); },
        });
    }
    {
        const encodedOffset = uleb(0xfffffff8);
        cases.push({
            name: `out-of-bounds i64.store offset ${0xfffffff8} encoded as ${hex(encodedOffset)}`,
            params: [I32, I64], results: [], locals: [],
            body: [Op.localGet, 0, Op.localGet, 1, Op.i64Store, ...uleb(3, 5), ...encodedOffset],
            test(fn, label) { checkTraps(label, i => fn(i & 7, BigInt(i))); },
        });
    }
    runGroup("memory", cases, { importsMemory: true });
}

// memory64 offsets are u64s, so they can take up to 10 bytes. Offsets of 2^35 and above have payload
// bits past the fifth byte; with a one-page memory they must trap rather than wrap to a small offset.
{
    const value = i => Math.imul(i + 1, 0x9e3779b1) | 0;
    const address = i => BigInt(8 * (i & 7));
    const cases = [];
    for (const offset of [0n, 128n, 65000n]) {
        for (const length of lengthsFrom(minimalUnsignedLEBLength(offset), 10)) {
            const encodedOffset = uleb64(offset, length);
            cases.push({
                name: `memory64 i32.store and i32.load offset ${offset} encoded as ${hex(encodedOffset)}`,
                params: [I64, I32], results: [I32], locals: [],
                body: [
                    Op.localGet, 0, Op.localGet, 1, Op.i32Store, 2, ...encodedOffset,
                    Op.localGet, 0, Op.i32Load, 2, ...encodedOffset,
                ],
                test(fn, label) { check(label, i => fn(address(i), value(i)), value); },
            });
        }
    }
    for (const offset of [1n << 35n, (1n << 35n) + 8n, 1n << 42n, (1n << 49n) + 4n, 1n << 56n, 1n << 63n, (1n << 64n) - 1n]) {
        for (const length of lengthsFrom(minimalUnsignedLEBLength(offset), 10)) {
            const encodedOffset = uleb64(offset, length);
            cases.push({
                name: `out-of-bounds memory64 i32.load offset ${offset} encoded as ${hex(encodedOffset)}`,
                params: [I64], results: [I32], locals: [],
                body: [Op.localGet, 0, Op.i32Load, 2, ...encodedOffset],
                test(fn, label) { checkTraps(label, i => fn(address(i))); },
            });
        }
    }
    runGroup("memory64", cases, { memory64: true });
}

// 0xfc-prefixed saturating truncations with padded sub-opcodes.
{
    const f32Inputs = [0, 1.5, -1.5, 123456.75, 2147483520, 3e9, -3e9, 5e9, NaN, Infinity, -Infinity, -0.5];
    const f64Inputs = [0, 1.5, -1.5, 123456789.9, 1e18, -1e18, 1e19, -1e19, 9007199254740993, NaN, Infinity, -Infinity];

    const truncSatI32S = x => {
        x = Math.fround(x);
        if (Number.isNaN(x))
            return 0;
        if (x >= 2 ** 31)
            return INT32_MAX;
        if (x <= -(2 ** 31))
            return INT32_MIN;
        return Math.trunc(x) | 0;
    };
    const truncSatI32U = x => {
        x = Math.fround(x);
        if (Number.isNaN(x) || x <= -1)
            return 0;
        if (x >= 2 ** 32)
            return -1;
        return Math.trunc(x) | 0;
    };
    const truncSatI64S = x => {
        if (Number.isNaN(x))
            return 0n;
        if (x >= 2 ** 63)
            return INT64_MAX;
        if (x <= -(2 ** 63))
            return INT64_MIN;
        return BigInt(Math.trunc(x));
    };

    const ops = [
        { name: "i32.trunc_sat_f32_s", subOpcode: FCOp.i32TruncSatF32S, param: F32, result: I32, inputs: f32Inputs, expected: truncSatI32S },
        { name: "i32.trunc_sat_f32_u", subOpcode: FCOp.i32TruncSatF32U, param: F32, result: I32, inputs: f32Inputs, expected: truncSatI32U },
        { name: "i64.trunc_sat_f64_s", subOpcode: FCOp.i64TruncSatF64S, param: F64, result: I64, inputs: f64Inputs, expected: truncSatI64S },
    ];
    const cases = [];
    for (const op of ops) {
        for (const length of lengthsFrom(1, 5)) {
            const subOpcode = uleb(op.subOpcode, length);
            cases.push({
                name: `${op.name} with sub-opcode encoded as ${hex(subOpcode)}`,
                params: [op.param], results: [op.result], locals: [],
                body: [Op.localGet, 0, Op.prefixFC, ...subOpcode],
                test(fn, label) {
                    const input = i => op.inputs[i % op.inputs.length];
                    check(label, i => fn(input(i)), i => op.expected(input(i)));
                },
            });
        }
    }
    runGroup("0xfc prefix", cases);
}

// 0xfb-prefixed i31 operations with padded sub-opcodes.
{
    const gcProbe = buildModule([{
        params: [I32], results: [I32], locals: [],
        body: [Op.localGet, 0, Op.prefixGC, GCOp.refI31, Op.prefixGC, GCOp.i31GetS],
    }]);
    if (WebAssembly.validate(gcProbe)) {
        const input = i => Math.imul(i + 1, 0x9e3779b1) | 0;
        const cases = [];
        for (const length of lengthsFrom(1, 5)) {
            const refI31 = uleb(GCOp.refI31, length);
            const getS = uleb(GCOp.i31GetS, 6 - length);
            const getU = uleb(GCOp.i31GetU, length);
            cases.push({
                name: `ref.i31 encoded as ${hex(refI31)}, i31.get_s encoded as ${hex(getS)}`,
                params: [I32], results: [I32], locals: [],
                body: [Op.localGet, 0, Op.prefixGC, ...refI31, Op.prefixGC, ...getS],
                test(fn, label) { check(label, i => fn(input(i)), i => (input(i) << 1) >> 1); },
            });
            cases.push({
                name: `ref.i31 encoded as ${hex(refI31)}, i31.get_u encoded as ${hex(getU)}`,
                params: [I32], results: [I32], locals: [],
                body: [Op.localGet, 0, Op.prefixGC, ...refI31, Op.prefixGC, ...getU],
                test(fn, label) { check(label, i => fn(input(i)), i => input(i) & 0x7fffffff); },
            });
        }
        runGroup("0xfb prefix", cases);
    }
}

// 0xfe-prefixed atomics with padded sub-opcodes, followed by both one-byte and multi-byte memargs. The
// handlers find the memarg where the sub-opcode ends.
{
    const base = 4096;
    const address = i => base + 8 * (i & 7);
    const memargs = [
        { alignmentLength: 1, offset: 0, offsetLength: 1 },
        { alignmentLength: 1, offset: 128, offsetLength: 2 },
        { alignmentLength: 3, offset: 70000, offsetLength: 5 },
    ];
    for (const { offset } of memargs) {
        for (let i = base + offset; i < base + offset + 72; ++i)
            view.setUint8(i, Math.imul(i + 1, 0x9e3779b1) >>> 24);
    }

    const value = i => Math.imul(i + 1, 0x85ebca6b) | 0;
    const ops = [
        {
            name: "i32.atomic.load", subOpcode: AtomicOp.i32AtomicLoad, alignment: 2,
            params: [I32], results: [I32], operands: [Op.localGet, 0],
            test: (fn, offset) => i => fn(address(i)) === view.getInt32(address(i) + offset, true),
        },
        {
            name: "i64.atomic.load", subOpcode: AtomicOp.i64AtomicLoad, alignment: 3,
            params: [I32], results: [I64], operands: [Op.localGet, 0],
            test: (fn, offset) => i => fn(address(i)) === view.getBigInt64(address(i) + offset, true),
        },
        {
            name: "i32.atomic.store", subOpcode: AtomicOp.i32AtomicStore, alignment: 2,
            params: [I32, I32], results: [], operands: [Op.localGet, 0, Op.localGet, 1],
            test: (fn, offset) => i => {
                fn(address(i), value(i));
                return view.getInt32(address(i) + offset, true) === value(i);
            },
        },
        {
            name: "i32.atomic.rmw.add", subOpcode: AtomicOp.i32AtomicRmwAdd, alignment: 2,
            params: [I32, I32], results: [I32], operands: [Op.localGet, 0, Op.localGet, 1],
            test: (fn, offset) => i => {
                const before = view.getInt32(address(i) + offset, true);
                return fn(address(i), value(i)) === before && view.getInt32(address(i) + offset, true) === ((before + value(i)) | 0);
            },
        },
    ];

    const cases = [];
    for (const op of ops) {
        for (const length of lengthsFrom(1, 5)) {
            const subOpcode = uleb(op.subOpcode, length);
            for (const { alignmentLength, offset, offsetLength } of memargs) {
                const memarg = [...uleb(op.alignment, alignmentLength), ...uleb(offset, offsetLength)];
                cases.push({
                    name: `${op.name} encoded as ${hex(subOpcode)} with memarg ${hex(memarg)}`,
                    params: op.params, results: op.results, locals: [],
                    body: [...op.operands, Op.prefixAtomic, ...subOpcode, ...memarg],
                    test(fn, label) { check(label, op.test(fn, offset), () => true); },
                });
            }
        }
    }
    for (const length of lengthsFrom(1, 5)) {
        const fence = uleb(AtomicOp.fence, length);
        cases.push({
            name: `atomic.fence encoded as ${hex(fence)}`,
            params: [I32], results: [I32], locals: [],
            body: [Op.prefixAtomic, ...fence, 0x00, Op.localGet, 0],
            test(fn, label) { check(label, i => fn(value(i)), value); },
        });
    }
    runGroup("0xfe prefix", cases, { importsMemory: true });
}

// SIMD sub-opcodes, including i32x4.add whose sub-opcode needs 2 bytes even when minimally encoded.
{
    const simdProbe = buildModule([{
        params: [], results: [I32], locals: [],
        body: [Op.prefixSIMD, SIMDOp.v128Const, ...new Array(16).fill(0), Op.prefixSIMD, SIMDOp.i32x4ExtractLane, 0],
    }]);
    if (WebAssembly.validate(simdProbe)) {
        const i32Lane = i => Math.imul(i + 1, 0x9e3779b1) | 0;
        const lanes = [0x01234567, 0x76543210, 0x0badf00d, 0x89abcdef];
        const laneBytes = lanes.flatMap(lane => [0, 8, 16, 24].map(shift => (lane >>> shift) & 0xff));
        const cases = [];
        for (const length of lengthsFrom(1, 5)) {
            const splat = uleb(SIMDOp.i32x4Splat, length);
            const extractLane = uleb(SIMDOp.i32x4ExtractLane, 6 - length);
            cases.push({
                name: `i32x4.splat encoded as ${hex(splat)}, i32x4.extract_lane encoded as ${hex(extractLane)}`,
                params: [I32], results: [I32], locals: [],
                body: [Op.localGet, 0, Op.prefixSIMD, ...splat, Op.prefixSIMD, ...extractLane, length % 4],
                test(fn, label) { check(label, i => fn(i32Lane(i)), i => i32Lane(i)); },
            });

            const v128Const = uleb(SIMDOp.v128Const, length);
            const lane = length % 4;
            cases.push({
                name: `v128.const encoded as ${hex(v128Const)}, extracting lane ${lane}`,
                params: [], results: [I32], locals: [],
                body: [Op.prefixSIMD, ...v128Const, ...laneBytes, Op.prefixSIMD, SIMDOp.i32x4ExtractLane, lane],
                test(fn, label) { check(label, () => fn(), () => lanes[lane] | 0); },
            });
        }
        for (const length of lengthsFrom(minimalUnsignedLEBLength(SIMDOp.i32x4Add), 5)) {
            const add = uleb(SIMDOp.i32x4Add, length);
            cases.push({
                name: `i32x4.add encoded as ${hex(add)}`,
                params: [I32], results: [I32], locals: [],
                body: [
                    Op.localGet, 0, Op.prefixSIMD, SIMDOp.i32x4Splat,
                    Op.localGet, 0, Op.prefixSIMD, SIMDOp.i32x4Splat,
                    Op.prefixSIMD, ...add,
                    Op.prefixSIMD, SIMDOp.i32x4ExtractLane, 1,
                ],
                test(fn, label) { check(label, i => fn(i32Lane(i)), i => (2 * i32Lane(i)) | 0); },
            });
        }
        runGroup("SIMD", cases);
    }
}
