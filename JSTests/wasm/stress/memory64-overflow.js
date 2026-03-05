//@ skip if $addressBits <= 32
//@ runDefaultWasm("--useWasmMemory64=1", "--useOMGJIT=0")

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
