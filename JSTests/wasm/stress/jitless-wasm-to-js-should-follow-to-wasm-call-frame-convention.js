//@ runDefault("--jitPolicyScale=0.1", "--useJIT=0", "--watchdog-exception-ok", "--watchdog=500")

function instantiate(moduleBase64, importObject) {
    let bytes = Uint8Array.fromBase64(moduleBase64);
    return WebAssembly.instantiate(bytes, importObject);
  }
  const report = $.agent.report;
  const isJIT = callerIsBBQOrOMGCompiled;
const extra = {isJIT};
(async function () {
/**
@param {I64} a0
@returns {I64}
 */
let fn0 = function (a0) {
a0?.toString();
return 1325n;
};
/**
@param {I64} a0
@returns {I64}
 */
let fn1 = function (a0) {
a0?.toString();
return 1602n;
};
/**
@param {I64} a0
@returns {void}
 */
let fn2 = function (a0) {
a0?.toString();
};
/**
@param {I64} a0
@returns {[FuncRef, I32]}
 */
let fn3 = function (a0) {
a0?.toString();
return [null, 47];
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
@returns {I64}
 */
let fn5 = function (a0) {
a0?.toString();
return 119n;
};
/**
@param {I64} a0
@returns {void}
 */
let fn6 = function (a0) {
a0?.toString();
};
/**
@param {I64} a0
@returns {void}
 */
let fn7 = function (a0) {
a0?.toString();
};
let tag1 = new WebAssembly.Tag({parameters: ['i64']});
let global0 = new WebAssembly.Global({value: 'i32', mutable: true}, 2663374967);
let table0 = new WebAssembly.Table({initial: 12, element: 'anyfunc'});
let table1 = new WebAssembly.Table({initial: 71, element: 'anyfunc'});
let table3 = new WebAssembly.Table({initial: 22, element: 'externref'});
let table4 = new WebAssembly.Table({initial: 49, element: 'externref'});
let table5 = new WebAssembly.Table({initial: 86, element: 'externref'});
let table7 = new WebAssembly.Table({initial: 92, element: 'anyfunc'});
let m0 = {fn0, fn6, global0, table0};
let m1 = {fn1, fn3, fn4, fn5, fn7, table5};
let m2 = {fn2, table1, table2: table1, table3, table4, table6: table5, table7, tag1};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('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', importObject0);
let {fn8, fn9, global1, global2, global3, global4, global5, memory0, table8, tag0} = /**
  @type {{
fn8: (a0: I64) => [FuncRef, I32],
fn9: (a0: I64) => I64,
global1: WebAssembly.Global,
global2: WebAssembly.Global,
global3: WebAssembly.Global,
global4: WebAssembly.Global,
global5: WebAssembly.Global,
memory0: WebAssembly.Memory,
table8: WebAssembly.Table,
tag0: WebAssembly.Tag
  }} */ (i0.instance.exports);
table3.set(4, table5);
table5.set(58, table5);
table5.set(18, table3);
table5.set(37, table4);
global2.value = 0;
global1.value = 0;
global3.value = null;
report('progress');
try {
  for (let k=0; k<26; k++) {
  let zzz = fn9(1329794613271303928n);
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
report('progress');
try {
  for (let k=0; k<16; k++) {
  let zzz = fn8(-2254323512825350491n);
  if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
if (zzz.length != 2) { throw new Error('expected array of length 2 but return value is '+zzz); }
let [r0, r1] = zzz;
r0?.toString(); r1?.toString();
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
  let zzz = fn9(3633256515183120413n);
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
report('progress');
try {
  for (let k=0; k<24; k++) {
  let zzz = fn8(6267585709684061258n);
  if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
if (zzz.length != 2) { throw new Error('expected array of length 2 but return value is '+zzz); }
let [r0, r1] = zzz;
r0?.toString(); r1?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
let tables = [table3, table5, table4, table1, table0, table7, table8];
for (let table of tables) {
for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
}
})().then(() => {
  report('after');
}).catch(e => {
  report('error');
})
