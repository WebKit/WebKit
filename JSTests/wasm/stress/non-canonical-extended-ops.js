import * as assert from "../assert.js";
load("../v8/resources/wasm-module-builder.js", "caller relative");

// Each function body encodes its 0xfc misc-op sub-opcode with a NON-CANONICAL
// (multi-byte padded) unsigned LEB128 -- e.g. [0x80, 0x00] or [0x80, 0x80, 0x00]
// instead of the canonical single byte -- to check that JSC accepts overlong
// LEB encodings of the misc-op sub-opcode. The raw bytes are passed straight to
// addBody(); a WAT assembler would only ever emit the canonical short form.
const instance = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction('I32TruncSatF32S_2', makeSig([kWasmF32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x80, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF32S_3', makeSig([kWasmF32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x80, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I32TruncSatF32U_2', makeSig([kWasmF32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x81, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF32U_3', makeSig([kWasmF32], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x81, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I32TruncSatF64S_2', makeSig([kWasmF64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x82, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF64S_3', makeSig([kWasmF64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x82, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I32TruncSatF64U_2', makeSig([kWasmF64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x83, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF64U_3', makeSig([kWasmF64], [kWasmI32]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x83, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF32S_2', makeSig([kWasmF32], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x84, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF32S_3', makeSig([kWasmF32], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x84, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF32U_2', makeSig([kWasmF32], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x85, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF32U_3', makeSig([kWasmF32], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x85, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF64S_2', makeSig([kWasmF64], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x86, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF64S_3', makeSig([kWasmF64], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x86, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF64U_2', makeSig([kWasmF64], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x87, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF64U_3', makeSig([kWasmF64], [kWasmI64]))
    .addBody([kExprLocalGet, 0, 0xfc /* Misc ops prefix */, 0x87, 0x80, 0x00])
    .exportFunc();

  return builder.instantiate({});
})();

assert.eq(instance.exports.I32TruncSatF32S_2(1), 1);
assert.eq(instance.exports.I32TruncSatF32S_3(1), 1);

assert.eq(instance.exports.I32TruncSatF32U_2(1), 1);
assert.eq(instance.exports.I32TruncSatF32U_3(1), 1);

assert.eq(instance.exports.I32TruncSatF64S_2(1), 1);
assert.eq(instance.exports.I32TruncSatF64S_3(1), 1);

assert.eq(instance.exports.I32TruncSatF64U_2(1), 1);
assert.eq(instance.exports.I32TruncSatF64U_3(1), 1);

assert.eq(instance.exports.I64TruncSatF32S_2(1), 1n);
assert.eq(instance.exports.I64TruncSatF32S_3(1), 1n);

assert.eq(instance.exports.I64TruncSatF32U_2(1), 1n);
assert.eq(instance.exports.I64TruncSatF32U_3(1), 1n);

assert.eq(instance.exports.I64TruncSatF64S_2(1), 1n);
assert.eq(instance.exports.I64TruncSatF64S_3(1), 1n);

assert.eq(instance.exports.I64TruncSatF64U_2(1), 1n);
assert.eq(instance.exports.I64TruncSatF64U_3(1), 1n);
