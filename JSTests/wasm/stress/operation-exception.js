function instantiate(moduleBase64, importObject, compileOptions) {
    let bytes = Uint8Array.fromBase64(moduleBase64);
    if (compileOptions) {
      return WebAssembly.instantiate(bytes, importObject, compileOptions);
    }
    return WebAssembly.instantiate(bytes, importObject);
  }
  const report = $.agent.report;
  var isJIT = callerIsBBQOrOMGCompiled;
const extra = {isJIT};
(async function () {
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
@returns {I32}
 */
let fn3 = function () {

return 59;
};
/**
@returns {void}
 */
let fn4 = function () {
};
/**
@returns {void}
 */
let fn5 = function () {
};
/**
@returns {[FuncRef, F32, I64]}
 */
let fn6 = function () {

return [null, 8.41149755487738e-42, 19n];
};
/**
@returns {void}
 */
let fn7 = function () {
};
/**
@returns {I32}
 */
let fn8 = function () {

return 42;
};
let tag5 = new WebAssembly.Tag({parameters: []});
let tag11 = new WebAssembly.Tag({parameters: []});
let table0 = new WebAssembly.Table({initial: 71, element: 'anyfunc', maximum: 505});
let m1 = {fn0, fn2, fn3, fn6, fn8, tag5, tag8: tag5, tag9: tag5, tag11, tag12: tag11};
let m0 = {fn1, fn4, fn5, fn7, tag10: tag5, tag13: tag5};
let m2 = {table0, tag6: tag5, tag7: tag5};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('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', importObject0);
let {fn9, fn10, global0, global1, memory0, table1, tag0, tag1, tag2, tag3, tag4} = /**
  @type {{
fn9: () => void,
fn10: () => I32,
global0: WebAssembly.Global,
global1: WebAssembly.Global,
memory0: WebAssembly.Memory,
table1: WebAssembly.Table,
tag0: WebAssembly.Tag,
tag1: WebAssembly.Tag,
tag2: WebAssembly.Tag,
tag3: WebAssembly.Tag,
tag4: WebAssembly.Tag
  }} */ (i0.instance.exports);
global1.value = null;
report('progress');
try {
  for (let k=0; k<20; k++) {
  let zzz = fn9();
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
  for (let k=0; k<29; k++) {
  let zzz = fn10();
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
  for (let k=0; k<13; k++) {
  let zzz = fn9();
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
  let zzz = fn10();
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
let tables = [table0, table1];
for (let table of tables) {
for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
}
})().then(() => {
  report('after');
}).catch(e => {
  report('error');
})
