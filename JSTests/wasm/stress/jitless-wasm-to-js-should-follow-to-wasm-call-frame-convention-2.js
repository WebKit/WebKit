//@ skip if $addressBits <= 32
//@ runDefault("--jitPolicyScale=0.1", "--useJIT=0", "--watchdog-exception-ok", "--watchdog=500")
function instantiate(moduleBase64, importObject) {
    let bytes = Uint8Array.fromBase64(moduleBase64);
    return WebAssembly.instantiate(bytes, importObject);
  }
  const report = $.agent.report;
  const isJIT = callerIsBBQOrOMGCompiled;
const extra = {isJIT};
(async function () {
let memory0 = new WebAssembly.Memory({initial: 3320, shared: true, maximum: 5066});
/**
@param {ExternRef} a0
@param {I64} a1
@param {FuncRef} a2
@returns {[ExternRef, I64, FuncRef]}
 */
let fn0 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [a0, 1661n, a2];
};
/**
@param {ExternRef} a0
@param {ExternRef} a1
@param {I64} a2
@returns {[FuncRef, I64]}
 */
let fn1 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [null, 1462n];
};
/**
@param {I64} a0
@returns {void}
 */
let fn2 = function (a0) {
a0?.toString();
};
/**
@param {ExternRef} a0
@param {I64} a1
@param {FuncRef} a2
@returns {[ExternRef, I64, FuncRef]}
 */
let fn3 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [a0, 77n, a2];
};
/**
@param {I64} a0
@returns {void}
 */
let fn4 = function (a0) {
a0?.toString();
};
/**
@param {I64} a0
@returns {void}
 */
let fn5 = function (a0) {
a0?.toString();
};
/**
@param {ExternRef} a0
@param {ExternRef} a1
@param {I64} a2
@returns {[ExternRef, ExternRef, I64]}
 */
let fn6 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [a0, a0, 759n];
};
/**
@param {I64} a0
@returns {void}
 */
let fn7 = function (a0) {
a0?.toString();
};
/**
@param {ExternRef} a0
@param {I64} a1
@param {FuncRef} a2
@returns {void}
 */
let fn8 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
};
let tag6 = new WebAssembly.Tag({parameters: ['externref', 'externref', 'i64']});
let tag7 = new WebAssembly.Tag({parameters: ['i64']});
let tag9 = new WebAssembly.Tag({parameters: ['externref', 'i64', 'anyfunc']});
let global0 = new WebAssembly.Global({value: 'i64', mutable: true}, 2341610737n);
let global1 = new WebAssembly.Global({value: 'f32', mutable: true}, 379045.1329523022);
let table0 = new WebAssembly.Table({initial: 68, element: 'externref'});
let table1 = new WebAssembly.Table({initial: 65, element: 'externref', maximum: 375});
let table3 = new WebAssembly.Table({initial: 5, element: 'anyfunc', maximum: 495});
let table4 = new WebAssembly.Table({initial: 72, element: 'anyfunc', maximum: 72});
let table6 = new WebAssembly.Table({initial: 88, element: 'externref'});
let m0 = {fn0, fn4, fn5, fn6, memory0, table5: table4, tag10: tag9, tag11: tag9};
let m2 = {fn1, fn2, fn3, global0, global1, global3: global1, global4: global0, table2: table0, table6, tag6, tag7};
let m1 = {fn7, fn8, global2: 499464717n, table0, table1, table3, table4, tag8: tag6, tag9};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('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', importObject0);
let {fn9, fn10, global5, global6, global7, global8, global9, global10, global11, global12, global13, global14, global15, global16, memory1, table7, table8, table9, table10, table11, table12, tag0, tag1, tag2, tag3, tag4, tag5} = /**
  @type {{
fn9: (a0: I64) => void,
fn10: (a0: ExternRef, a1: ExternRef, a2: I64) => [ExternRef, ExternRef, I64],
global5: WebAssembly.Global,
global6: WebAssembly.Global,
global7: WebAssembly.Global,
global8: WebAssembly.Global,
global9: WebAssembly.Global,
global10: WebAssembly.Global,
global11: WebAssembly.Global,
global12: WebAssembly.Global,
global13: WebAssembly.Global,
global14: WebAssembly.Global,
global15: WebAssembly.Global,
global16: WebAssembly.Global,
memory1: WebAssembly.Memory,
table7: WebAssembly.Table,
table8: WebAssembly.Table,
table9: WebAssembly.Table,
table10: WebAssembly.Table,
table11: WebAssembly.Table,
table12: WebAssembly.Table,
tag0: WebAssembly.Tag,
tag1: WebAssembly.Tag,
tag2: WebAssembly.Tag,
tag3: WebAssembly.Tag,
tag4: WebAssembly.Tag,
tag5: WebAssembly.Tag
  }} */ (i0.instance.exports);
table9.set(6, table9);
table9.set(17, table1);
table9.set(6, table6);
table0.set(61, table6);
table1.set(16, table0);
table6.set(33, table0);
table9.set(58, table0);
table9.set(47, table9);
table9.set(63, table6);
table9.set(56, table6);
table6.set(65, table1);
table6.set(39, table6);
table6.set(50, table9);
table9.set(14, table1);
table6.set(18, table1);
table0.set(59, table1);
table0.set(27, table6);
table6.set(58, table1);
table6.set(64, table0);
table9.set(88, table6);
global14.value = 0n;
global16.value = null;
report('progress');
try {
  for (let k=0; k<11; k++) {
  let zzz = fn10(global10.value, global10.value, global8.value);
  if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
if (zzz.length != 3) { throw new Error('expected array of length 3 but return value is '+zzz); }
let [r0, r1, r2] = zzz;
r0?.toString(); r1?.toString(); r2?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
report('progress');
try {
  for (let k=0; k<23; k++) {
  let zzz = fn9(global8.value);
  if (zzz !== undefined) { throw new Error('expected undefined but return value is '+zzz); }
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
report('progress');
try {
  for (let k=0; k<13; k++) {
  let zzz = fn9(global14.value);
  if (zzz !== undefined) { throw new Error('expected undefined but return value is '+zzz); }
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
report('progress');
try {
  for (let k=0; k<8; k++) {
  let zzz = fn10(global10.value, global13.value, global8.value);
  if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
if (zzz.length != 3) { throw new Error('expected array of length 3 but return value is '+zzz); }
let [r0, r1, r2] = zzz;
r0?.toString(); r1?.toString(); r2?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
let tables = [table0, table1, table6, table9, table4, table3, table10, table7, table12, table11, table8];
for (let table of tables) {
for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
}
})().then(() => {
  report('after');
}).catch(e => {
  report('error');
})
