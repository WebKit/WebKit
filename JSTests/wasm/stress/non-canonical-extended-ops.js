load("../spec-harness.js", "caller relative");

instance = (() => {
  let builder = new WasmModuleBuilder();
  builder.addFunction('I32TruncSatF32S_2', makeSig([kWasmF32], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x80, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF32S_3', makeSig([kWasmF32], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x80, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I32TruncSatF32U_2', makeSig([kWasmF32], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x81, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF32U_3', makeSig([kWasmF32], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x81, 0x80, 0x00])
    .exportFunc();


  builder.addFunction('I32TruncSatF64S_2', makeSig([kWasmF64], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x82, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF64S_3', makeSig([kWasmF64], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x82, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I32TruncSatF64U_2', makeSig([kWasmF64], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x83, 0x00])
    .exportFunc();
  builder.addFunction('I32TruncSatF64U_3', makeSig([kWasmF64], [kWasmI32]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x83, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF32S_2', makeSig([kWasmF32], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x84, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF32S_3', makeSig([kWasmF32], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x84, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF32U_2', makeSig([kWasmF32], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x85, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF32U_3', makeSig([kWasmF32], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x85, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF64S_2', makeSig([kWasmF64], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x86, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF64S_3', makeSig([kWasmF64], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x86, 0x80, 0x00])
    .exportFunc();

  builder.addFunction('I64TruncSatF64U_2', makeSig([kWasmF64], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x87, 0x00])
    .exportFunc();
  builder.addFunction('I64TruncSatF64U_3', makeSig([kWasmF64], [kWasmI64]))
    .addBody([
      kExprGetLocal, 0,
      0xfc /* Misc ops prefix */, 0x87, 0x80, 0x00])
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

