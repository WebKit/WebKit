//@ skip if $addressBits <= 32
//@ runDefaultWasm("--useWasmMemory64=1")

load("../spec-harness.js", "caller relative");

// u64 LEB128 encoding of 0xffffffffffffffff
const kU64MaxLEB128 = [0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0x01];

const builder = new WasmModuleBuilder();
builder.addMemory64(1);

builder.addFunction("testLoad", makeSig([kWasmI64], [kWasmI32]))
    .addBody([
        kExprGetLocal, 0,
        kExprI32LoadMem, 0, 1,
    ])
    .exportAs("testLoad");

builder.addFunction("testStore", makeSig([kWasmI64], []))
    .addBody([
        kExprGetLocal, 0,
        kExprI32Const, 0,
        kExprI32StoreMem, 0, 1,         
    ])
    .exportAs("testStore");

builder.addFunction("constantLoad", makeSig([], [kWasmI32]))
    .addBody([
        kExprI64Const, 0x7F,  // -1 as signed LEB128 = 0xFFFFFFFFFFFFFFFF
        kExprI32LoadMem, 0, 1,
    ])
    .exportAs("constantLoad");

builder.addFunction("constantStore", makeSig([], []))
    .addBody([
        kExprI64Const, 0x7F,  // -1 as signed LEB128 = 0xFFFFFFFFFFFFFFFF
        kExprI32Const, 0,
        kExprI32StoreMem, 0, 1,
    ])
    .exportAs("constantStore");

builder.addFunction("offsetLoad", makeSig([], [kWasmI32]))
    .addBody([
        kExprI64Const, 0,
        kExprI32LoadMem, 0, ...kU64MaxLEB128,  
    ])
    .exportAs("offsetLoad");

builder.addFunction("offsetStore", makeSig([], []))
    .addBody([
        kExprI64Const, 0,
        kExprI32Const, 0,
        kExprI32StoreMem, 0, ...kU64MaxLEB128,  
    ])
    .exportAs("offsetStore");

const { testLoad, testStore, constantLoad, constantStore, offsetStore, offsetLoad } = builder.instantiate().exports;

function testOverflow() {
  assert.throws(() => testLoad(0xffffffffffffffffn), 
      WebAssembly.RuntimeError, 
      "Out of bounds memory access (evaluating 'testLoad(0xffffffffffffffffn)')");

  assert.throws(() => testStore(0xffffffffffffffffn), 
        WebAssembly.RuntimeError, 
      "Out of bounds memory access (evaluating 'testStore(0xffffffffffffffffn)')");

  assert.throws(() => testLoad(0xfffffffffffffffen), 
      WebAssembly.RuntimeError, 
      "Out of bounds memory access (evaluating 'testLoad(0xfffffffffffffffen)')");

  assert.throws(() => testStore(0xfffffffffffffffen), 
        WebAssembly.RuntimeError, 
      "Out of bounds memory access (evaluating 'testStore(0xfffffffffffffffen)')");

  assert.throws(() => constantLoad(), 
        WebAssembly.RuntimeError, 
        "Out of bounds memory access (evaluating 'constantLoad()')");

  assert.throws(() => constantStore(), 
        WebAssembly.RuntimeError, 
        "Out of bounds memory access (evaluating 'constantStore()')");

  assert.throws(() => offsetStore(), 
        WebAssembly.RuntimeError, 
        "Out of bounds memory access (evaluating 'offsetStore()')");

    assert.throws(() => offsetLoad(), 
        WebAssembly.RuntimeError, 
        "Out of bounds memory access (evaluating 'offsetLoad()')");
}

for (let i = 0; i < wasmTestLoopCount; i++)
    testOverflow();

// Atomic instruction overflow tests.
// Every atomic instruction with a memory offset must trap on address overflow.

const kAtomicPrefix = 0xFE;

const kExprAtomicNotify = 0x00;
const kExprI32AtomicWait = 0x01;
const kExprI64AtomicWait = 0x02;
const kExprI32AtomicLoad = 0x10;
const kExprI64AtomicLoad = 0x11;
const kExprI32AtomicLoad8U = 0x12;
const kExprI32AtomicLoad16U = 0x13;
const kExprI64AtomicLoad8U = 0x14;
const kExprI64AtomicLoad16U = 0x15;
const kExprI64AtomicLoad32U = 0x16;
const kExprI32AtomicStore = 0x17;
const kExprI64AtomicStore = 0x18;
const kExprI32AtomicStore8 = 0x19;
const kExprI32AtomicStore16 = 0x1a;
const kExprI64AtomicStore8 = 0x1b;
const kExprI64AtomicStore16 = 0x1c;
const kExprI64AtomicStore32 = 0x1d;
const kExprI32AtomicRmwAdd = 0x1e;
const kExprI64AtomicRmwAdd = 0x1f;
const kExprI32AtomicRmw8AddU = 0x20;
const kExprI32AtomicRmw16AddU = 0x21;
const kExprI64AtomicRmw8AddU = 0x22;
const kExprI64AtomicRmw16AddU = 0x23;
const kExprI64AtomicRmw32AddU = 0x24;
const kExprI32AtomicRmwSub = 0x25;
const kExprI64AtomicRmwSub = 0x26;
const kExprI32AtomicRmw8SubU = 0x27;
const kExprI32AtomicRmw16SubU = 0x28;
const kExprI64AtomicRmw8SubU = 0x29;
const kExprI64AtomicRmw16SubU = 0x2a;
const kExprI64AtomicRmw32SubU = 0x2b;
const kExprI32AtomicRmwAnd = 0x2c;
const kExprI64AtomicRmwAnd = 0x2d;
const kExprI32AtomicRmw8AndU = 0x2e;
const kExprI32AtomicRmw16AndU = 0x2f;
const kExprI64AtomicRmw8AndU = 0x30;
const kExprI64AtomicRmw16AndU = 0x31;
const kExprI64AtomicRmw32AndU = 0x32;
const kExprI32AtomicRmwOr = 0x33;
const kExprI64AtomicRmwOr = 0x34;
const kExprI32AtomicRmw8OrU = 0x35;
const kExprI32AtomicRmw16OrU = 0x36;
const kExprI64AtomicRmw8OrU = 0x37;
const kExprI64AtomicRmw16OrU = 0x38;
const kExprI64AtomicRmw32OrU = 0x39;
const kExprI32AtomicRmwXor = 0x3a;
const kExprI64AtomicRmwXor = 0x3b;
const kExprI32AtomicRmw8XorU = 0x3c;
const kExprI32AtomicRmw16XorU = 0x3d;
const kExprI64AtomicRmw8XorU = 0x3e;
const kExprI64AtomicRmw16XorU = 0x3f;
const kExprI64AtomicRmw32XorU = 0x40;
const kExprI32AtomicRmwXchg = 0x41;
const kExprI64AtomicRmwXchg = 0x42;
const kExprI32AtomicRmw8XchgU = 0x43;
const kExprI32AtomicRmw16XchgU = 0x44;
const kExprI64AtomicRmw8XchgU = 0x45;
const kExprI64AtomicRmw16XchgU = 0x46;
const kExprI64AtomicRmw32XchgU = 0x47;
const kExprI32AtomicRmwCmpxchg = 0x48;
const kExprI64AtomicRmwCmpxchg = 0x49;
const kExprI32AtomicRmw8CmpxchgU = 0x4a;
const kExprI32AtomicRmw16CmpxchgU = 0x4b;
const kExprI64AtomicRmw8CmpxchgU = 0x4c;
const kExprI64AtomicRmw16CmpxchgU = 0x4d;
const kExprI64AtomicRmw32CmpxchgU = 0x4e;

// Each entry: [name, sub-opcode, natural alignment (log2), category, value wasm type]
// Categories: "load", "store", "rmw", "cmpxchg", "notify"
const atomicOps = [
    // Atomic loads
    ["i32_atomic_load",     kExprI32AtomicLoad,     2, "load", kWasmI32],
    ["i64_atomic_load",     kExprI64AtomicLoad,     3, "load", kWasmI64],
    ["i32_atomic_load8_u",  kExprI32AtomicLoad8U,   0, "load", kWasmI32],
    ["i32_atomic_load16_u", kExprI32AtomicLoad16U,  1, "load", kWasmI32],
    ["i64_atomic_load8_u",  kExprI64AtomicLoad8U,   0, "load", kWasmI64],
    ["i64_atomic_load16_u", kExprI64AtomicLoad16U,  1, "load", kWasmI64],
    ["i64_atomic_load32_u", kExprI64AtomicLoad32U,  2, "load", kWasmI64],

    // Atomic stores
    ["i32_atomic_store",    kExprI32AtomicStore,    2, "store", kWasmI32],
    ["i64_atomic_store",    kExprI64AtomicStore,    3, "store", kWasmI64],
    ["i32_atomic_store8",   kExprI32AtomicStore8,   0, "store", kWasmI32],
    ["i32_atomic_store16",  kExprI32AtomicStore16,  1, "store", kWasmI32],
    ["i64_atomic_store8",   kExprI64AtomicStore8,   0, "store", kWasmI64],
    ["i64_atomic_store16",  kExprI64AtomicStore16,  1, "store", kWasmI64],
    ["i64_atomic_store32",  kExprI64AtomicStore32,  2, "store", kWasmI64],

    // Atomic RMW add
    ["i32_atomic_rmw_add",      kExprI32AtomicRmwAdd,     2, "rmw", kWasmI32],
    ["i64_atomic_rmw_add",      kExprI64AtomicRmwAdd,     3, "rmw", kWasmI64],
    ["i32_atomic_rmw8_add_u",   kExprI32AtomicRmw8AddU,   0, "rmw", kWasmI32],
    ["i32_atomic_rmw16_add_u",  kExprI32AtomicRmw16AddU,  1, "rmw", kWasmI32],
    ["i64_atomic_rmw8_add_u",   kExprI64AtomicRmw8AddU,   0, "rmw", kWasmI64],
    ["i64_atomic_rmw16_add_u",  kExprI64AtomicRmw16AddU,  1, "rmw", kWasmI64],
    ["i64_atomic_rmw32_add_u",  kExprI64AtomicRmw32AddU,  2, "rmw", kWasmI64],

    // Atomic RMW sub
    ["i32_atomic_rmw_sub",      kExprI32AtomicRmwSub,     2, "rmw", kWasmI32],
    ["i64_atomic_rmw_sub",      kExprI64AtomicRmwSub,     3, "rmw", kWasmI64],
    ["i32_atomic_rmw8_sub_u",   kExprI32AtomicRmw8SubU,   0, "rmw", kWasmI32],
    ["i32_atomic_rmw16_sub_u",  kExprI32AtomicRmw16SubU,  1, "rmw", kWasmI32],
    ["i64_atomic_rmw8_sub_u",   kExprI64AtomicRmw8SubU,   0, "rmw", kWasmI64],
    ["i64_atomic_rmw16_sub_u",  kExprI64AtomicRmw16SubU,  1, "rmw", kWasmI64],
    ["i64_atomic_rmw32_sub_u",  kExprI64AtomicRmw32SubU,  2, "rmw", kWasmI64],

    // Atomic RMW and
    ["i32_atomic_rmw_and",      kExprI32AtomicRmwAnd,     2, "rmw", kWasmI32],
    ["i64_atomic_rmw_and",      kExprI64AtomicRmwAnd,     3, "rmw", kWasmI64],
    ["i32_atomic_rmw8_and_u",   kExprI32AtomicRmw8AndU,   0, "rmw", kWasmI32],
    ["i32_atomic_rmw16_and_u",  kExprI32AtomicRmw16AndU,  1, "rmw", kWasmI32],
    ["i64_atomic_rmw8_and_u",   kExprI64AtomicRmw8AndU,   0, "rmw", kWasmI64],
    ["i64_atomic_rmw16_and_u",  kExprI64AtomicRmw16AndU,  1, "rmw", kWasmI64],
    ["i64_atomic_rmw32_and_u",  kExprI64AtomicRmw32AndU,  2, "rmw", kWasmI64],

    // Atomic RMW or
    ["i32_atomic_rmw_or",       kExprI32AtomicRmwOr,      2, "rmw", kWasmI32],
    ["i64_atomic_rmw_or",       kExprI64AtomicRmwOr,      3, "rmw", kWasmI64],
    ["i32_atomic_rmw8_or_u",    kExprI32AtomicRmw8OrU,    0, "rmw", kWasmI32],
    ["i32_atomic_rmw16_or_u",   kExprI32AtomicRmw16OrU,   1, "rmw", kWasmI32],
    ["i64_atomic_rmw8_or_u",    kExprI64AtomicRmw8OrU,    0, "rmw", kWasmI64],
    ["i64_atomic_rmw16_or_u",   kExprI64AtomicRmw16OrU,   1, "rmw", kWasmI64],
    ["i64_atomic_rmw32_or_u",   kExprI64AtomicRmw32OrU,   2, "rmw", kWasmI64],

    // Atomic RMW xor
    ["i32_atomic_rmw_xor",      kExprI32AtomicRmwXor,     2, "rmw", kWasmI32],
    ["i64_atomic_rmw_xor",      kExprI64AtomicRmwXor,     3, "rmw", kWasmI64],
    ["i32_atomic_rmw8_xor_u",   kExprI32AtomicRmw8XorU,   0, "rmw", kWasmI32],
    ["i32_atomic_rmw16_xor_u",  kExprI32AtomicRmw16XorU,  1, "rmw", kWasmI32],
    ["i64_atomic_rmw8_xor_u",   kExprI64AtomicRmw8XorU,   0, "rmw", kWasmI64],
    ["i64_atomic_rmw16_xor_u",  kExprI64AtomicRmw16XorU,  1, "rmw", kWasmI64],
    ["i64_atomic_rmw32_xor_u",  kExprI64AtomicRmw32XorU,  2, "rmw", kWasmI64],

    // Atomic RMW xchg
    ["i32_atomic_rmw_xchg",     kExprI32AtomicRmwXchg,    2, "rmw", kWasmI32],
    ["i64_atomic_rmw_xchg",     kExprI64AtomicRmwXchg,    3, "rmw", kWasmI64],
    ["i32_atomic_rmw8_xchg_u",  kExprI32AtomicRmw8XchgU,  0, "rmw", kWasmI32],
    ["i32_atomic_rmw16_xchg_u", kExprI32AtomicRmw16XchgU, 1, "rmw", kWasmI32],
    ["i64_atomic_rmw8_xchg_u",  kExprI64AtomicRmw8XchgU,  0, "rmw", kWasmI64],
    ["i64_atomic_rmw16_xchg_u", kExprI64AtomicRmw16XchgU, 1, "rmw", kWasmI64],
    ["i64_atomic_rmw32_xchg_u", kExprI64AtomicRmw32XchgU, 2, "rmw", kWasmI64],

    // Atomic RMW cmpxchg
    ["i32_atomic_rmw_cmpxchg",      kExprI32AtomicRmwCmpxchg,     2, "cmpxchg", kWasmI32],
    ["i64_atomic_rmw_cmpxchg",      kExprI64AtomicRmwCmpxchg,     3, "cmpxchg", kWasmI64],
    ["i32_atomic_rmw8_cmpxchg_u",   kExprI32AtomicRmw8CmpxchgU,   0, "cmpxchg", kWasmI32],
    ["i32_atomic_rmw16_cmpxchg_u",  kExprI32AtomicRmw16CmpxchgU,  1, "cmpxchg", kWasmI32],
    ["i64_atomic_rmw8_cmpxchg_u",   kExprI64AtomicRmw8CmpxchgU,   0, "cmpxchg", kWasmI64],
    ["i64_atomic_rmw16_cmpxchg_u",  kExprI64AtomicRmw16CmpxchgU,  1, "cmpxchg", kWasmI64],
    ["i64_atomic_rmw32_cmpxchg_u",  kExprI64AtomicRmw32CmpxchgU,  2, "cmpxchg", kWasmI64],

    // memory.atomic.notify
    ["memory_atomic_notify", kExprAtomicNotify, 2, "notify", kWasmI32],

    // memory.atomic.wait32/wait64
    ["memory_atomic_wait32", kExprI32AtomicWait, 2, "wait32", kWasmI32],
    ["memory_atomic_wait64", kExprI64AtomicWait, 3, "wait64", kWasmI64],
];

function makeAtomicBody(cat, vtype, opcode, align, addrBytes, offsetBytes) {
    const body = [...addrBytes];
    const zeroConst = vtype === kWasmI64 ? [kExprI64Const, 0] : [kExprI32Const, 0];

    switch (cat) {
    case "store":
    case "rmw":
        body.push(...zeroConst);
        break;
    case "cmpxchg":
        body.push(...zeroConst, ...zeroConst);
        break;
    case "notify":
        body.push(kExprI32Const, 0);
        break;
    case "wait32":
        body.push(kExprI32Const, 0, kExprI64Const, 0);
        break;
    case "wait64":
        body.push(kExprI64Const, 0, kExprI64Const, 0);
        break;
    }

    body.push(kAtomicPrefix, opcode, align, ...offsetBytes);
    return body;
}

function resultTypeForCategory(cat, vtype) {
    if (cat === "store") return null;
    if (cat === "notify" || cat === "wait32" || cat === "wait64") return kWasmI32;
    return vtype;
}

const atomicBuilder = new WasmModuleBuilder();
atomicBuilder.addMemory64(1);

for (const [name, opcode, align, cat, vtype] of atomicOps) {
    const rtype = resultTypeForCategory(cat, vtype);
    const runtimeSig = rtype !== null ? makeSig([kWasmI64], [rtype]) : makeSig([kWasmI64], []);
    const staticSig = rtype !== null ? makeSig([], [rtype]) : makeSig([], []);

    atomicBuilder.addFunction("test_" + name, runtimeSig)
        .addBody(makeAtomicBody(cat, vtype, opcode, align, [kExprGetLocal, 0], [1]))
        .exportAs("test_" + name);

    atomicBuilder.addFunction("const_" + name, staticSig)
        .addBody(makeAtomicBody(cat, vtype, opcode, align, [kExprI64Const, 0x7F], [1]))
        .exportAs("const_" + name);

    atomicBuilder.addFunction("offset_" + name, staticSig)
        .addBody(makeAtomicBody(cat, vtype, opcode, align, [kExprI64Const, 0], kU64MaxLEB128))
        .exportAs("offset_" + name);
}

const atomicExports = atomicBuilder.instantiate().exports;

function testAtomicOverflow() {
    for (const [name] of atomicOps) {
        assert.throws(() => atomicExports["test_" + name](0xffffffffffffffffn),
            WebAssembly.RuntimeError,
            "Out of bounds memory access");

        assert.throws(() => atomicExports["test_" + name](0xfffffffffffffffen),
            WebAssembly.RuntimeError,
            "Out of bounds memory access");

        assert.throws(() => atomicExports["const_" + name](),
            WebAssembly.RuntimeError,
            "Out of bounds memory access");

        assert.throws(() => atomicExports["offset_" + name](),
            WebAssembly.RuntimeError,
            "Out of bounds memory access");
    }
}

for (let i = 0; i < wasmTestLoopCount; i++)
    testAtomicOverflow();
