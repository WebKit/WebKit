//@ $skipModes << "wasm-no-wasm-jit".to_sym
//@ $skipModes << "wasm-no-jit".to_sym
// BBQ / OMG is required
function instantiate(moduleBase64, importObject) {
    let bytes = Uint8Array.fromBase64(moduleBase64);
    return WebAssembly.instantiate(bytes, importObject);
  }
  const log = function () { };
  const report = $.agent.report;
  const isJIT = callerIsBBQOrOMGCompiled;
const extra = {isJIT};
(async function () {
/**
@param {I32} a0
@param {ExternRef} a1
@param {FuncRef} a2
@returns {[I32, ExternRef, FuncRef]}
 */
let fn0 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [30, a1, a2];
};
/**
@returns {I32}
 */
let fn1 = function () {

return 26;
};
/**
@param {I32} a0
@param {ExternRef} a1
@param {FuncRef} a2
@returns {[I32, ExternRef, FuncRef]}
 */
let fn2 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [64, a1, a2];
};
/**
@param {I32} a0
@param {ExternRef} a1
@param {FuncRef} a2
@returns {[I32, ExternRef, FuncRef]}
 */
let fn3 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [38, a1, a2];
};
/**
@param {I32} a0
@param {ExternRef} a1
@param {FuncRef} a2
@returns {void}
 */
let fn4 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
};
/**
@param {I32} a0
@param {ExternRef} a1
@param {FuncRef} a2
@returns {void}
 */
let fn5 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
};
let tag3 = new WebAssembly.Tag({parameters: ['i32', 'externref', 'anyfunc']});
let global0 = new WebAssembly.Global({value: 'externref', mutable: true}, {});
let global1 = new WebAssembly.Global({value: 'i32', mutable: true}, 1020505450);
let table0 = new WebAssembly.Table({initial: 51, element: 'externref', maximum: 506});
let table2 = new WebAssembly.Table({initial: 45, element: 'anyfunc', maximum: 45});
let m2 = {fn0, fn1, global0, table1: table0};
let m1 = {fn2, fn3, global1, tag3};
let m0 = {fn4, fn5, table0, table2};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('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', importObject0);
let {fn6, fn7, global2, global3, memory0, table3, table4, table5, table6, tag0, tag1, tag2} = /**
  @type {{
fn6: (a0: I32, a1: ExternRef, a2: FuncRef) => [I32, ExternRef, FuncRef],
fn7: () => I32,
global2: WebAssembly.Global,
global3: WebAssembly.Global,
memory0: WebAssembly.Memory,
table3: WebAssembly.Table,
table4: WebAssembly.Table,
table5: WebAssembly.Table,
table6: WebAssembly.Table,
tag0: WebAssembly.Tag,
tag1: WebAssembly.Tag,
tag2: WebAssembly.Tag
  }} */ (i0.instance.exports);
table0.set(30, table0);
table5.set(9, table4);
table5.set(4, table0);
table5.set(3, table0);
global1.value = 0;
global2.value = 0n;
global3.value = 0;
log('calling fn6');
report('progress');
try {
  for (let k=0; k<5; k++) {
  let zzz = fn6(global1.value, global0.value, null);
  if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
if (zzz.length != 3) { throw new Error('expected array of length 3 but return value is '+zzz); }
let [r0, r1, r2] = zzz;
r0?.toString(); r1?.toString(); r2?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
log('calling fn7');
report('progress');
try {
  for (let k=0; k<11; k++) {
  let zzz = fn7();
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
log('calling fn7');
report('progress');
try {
  for (let k=0; k<17; k++) {
  let zzz = fn7();
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
log('calling fn6');
report('progress');
try {
  for (let k=0; k<11; k++) {
  let zzz = fn6(global1.value, global0.value, null);
  if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
if (zzz.length != 3) { throw new Error('expected array of length 3 but return value is '+zzz); }
let [r0, r1, r2] = zzz;
r0?.toString(); r1?.toString(); r2?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
let tables = [table0, table4, table5, table2, table6, table3];
for (let table of tables) {
for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
}
})().then(() => {
  log('after')
  report('after');
}).catch(e => {
  log(e)
  log('error')
  report('error');
})
