//@ skip if !$isSIMDPlatform
let tools;
if (globalThis.callerIsBBQOrOMGCompiled) {
  function instantiateJsc(filename, importObject) {
    let bytes = read(filename, 'binary');
    return WebAssembly.instantiate(bytes, importObject, 'x');
  }
  const verbose = false;
  const nullLog = function () { }
  const log = verbose ? debug : nullLog;
  const report = verbose ? $.agent.report : nullLog;
  const isJIT = callerIsBBQOrOMGCompiled;
  tools = {log, report, isJIT, instantiate: instantiateJsc};
} else {
  function instantiateBrowser(filename, importObject) {
    let bytes = fetch(filename);
    return WebAssembly.instantiateStreaming(bytes, importObject, 'x');
  }
  const log = console.log;
  const report = console.log;
  const isJIT = () => 1;
  tools = {log, report, isJIT, instantiate: instantiateBrowser};
}
const {log, report, isJIT, instantiate} = tools;
const extra = {isJIT};
(async function () {
let memory0 = new WebAssembly.Memory({initial: 2890, shared: true, maximum: 4668});
/**
@returns {void}
 */
let fn0 = function () {
};
/**
@returns {void}
 */
let fn1 = function () {
};
/**
@returns {void}
 */
let fn2 = function () {
};
/**
@param {F32} a0
@param {FuncRef} a1
@returns {[F32, FuncRef]}
 */
let fn3 = function (a0, a1) {
a0?.toString(); a1?.toString();
return [2.0643424836123336e5, a1];
};
/**
@returns {I32}
 */
let fn4 = function () {

return 46;
};
/**
@param {F32} a0
@param {FuncRef} a1
@returns {void}
 */
let fn5 = function (a0, a1) {
a0?.toString(); a1?.toString();
};
/**
@param {F32} a0
@param {FuncRef} a1
@returns {void}
 */
let fn6 = function (a0, a1) {
a0?.toString(); a1?.toString();
};
/**
@param {I64} a0
@returns {I64}
 */
let fn7 = function (a0) {
a0?.toString();
return 1777n;
};
let tag5 = new WebAssembly.Tag({parameters: []});
let tag7 = new WebAssembly.Tag({parameters: ['i64']});
let tag8 = new WebAssembly.Tag({parameters: ['f32', 'anyfunc']});
let global0 = new WebAssembly.Global({value: 'i32', mutable: true}, 3511634618);
let global2 = new WebAssembly.Global({value: 'f32', mutable: true}, 839610.408920414);
let global4 = new WebAssembly.Global({value: 'f64', mutable: true}, 777737.2126211214);
let global5 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
let global6 = new WebAssembly.Global({value: 'f32', mutable: true}, 417311.1223992667);
let table0 = new WebAssembly.Table({initial: 3, element: 'anyfunc'});
let m0 = {fn5, global0, global3: global0, memory0, tag5, tag10: tag5};
let m2 = {fn0, fn4, fn6, fn7, global2, global4, global5, global6, tag6: tag5, tag7, tag9: tag5};
let m1 = {fn1, fn2, fn3, global1: global0, table0, tag8, tag11: tag5};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('try-and-block-with-v128-results.wasm', importObject0);
let {fn8, fn9, fn10, global7, global8, memory1, table1, tag0, tag1, tag2, tag3, tag4} = /**
  @type {{
fn8: (a0: I64) => I64,
fn9: (a0: F32, a1: FuncRef) => void,
fn10: (a0: I64) => void,
global7: WebAssembly.Global,
global8: WebAssembly.Global,
memory1: WebAssembly.Memory,
table1: WebAssembly.Table,
tag0: WebAssembly.Tag,
tag1: WebAssembly.Tag,
tag2: WebAssembly.Tag,
tag3: WebAssembly.Tag,
tag4: WebAssembly.Tag
  }} */ (i0.instance.exports);
global7.value = 0n;
global2.value = 0;
global4.value = 0;
global8.value = 'a';
// log('calling fn10');
report('progress');
try {
  for (let k=0; k<26; k++) {
  let zzz = fn10(global7.value);
  if (zzz !== undefined) { throw new Error('expected undefined but return value is '+zzz); }
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
// log('calling fn8');
report('progress');
try {
  for (let k=0; k<16; k++) {
  let zzz = fn8(global7.value);
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
// log('calling fn9');
report('progress');
try {
  for (let k=0; k<24; k++) {
  let zzz = fn9(global6.value, fn8);
  if (zzz !== undefined) { throw new Error('expected undefined but return value is '+zzz); }
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
// log('calling fn8');
report('progress');
try {
  for (let k=0; k<16; k++) {
  let zzz = fn8(global7.value);
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
// log('calling fn10');
report('progress');
try {
  for (let k=0; k<5; k++) {
  let zzz = fn10(global7.value);
  if (zzz !== undefined) { throw new Error('expected undefined but return value is '+zzz); }
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
// log('calling fn9');
report('progress');
try {
  for (let k=0; k<10; k++) {
  let zzz = fn9(global2.value, fn10);
  if (zzz !== undefined) { throw new Error('expected undefined but return value is '+zzz); }
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
let tables = [table0, table1];
for (let table of tables) {
for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
}
})().then(() => {
  // log('after')
  report('after');
}).catch(e => {
  log(e)
  log('error')
  report('error');
})
