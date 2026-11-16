function instantiate(moduleBase64, importObject) {
    let bytes = Uint8Array.fromBase64(moduleBase64);
    return WebAssembly.instantiate(bytes, importObject);
  }
// const report = $.agent.report;
const report = function() { };
const isJIT = callerIsBBQOrOMGCompiled;
const extra = {isJIT};
(async function () {
/**
@param {I64} a0
@param {I32} a1
@param {I32} a2
@returns {[I64, I32, I32]}
 */
let fn0 = function (a0, a1, a2) {
a0?.toString(); a1?.toString(); a2?.toString();
return [1574n, -10, -1];
};
let tag7 = new WebAssembly.Tag({parameters: ['i64', 'i32', 'i32']});
let tag8 = new WebAssembly.Tag({parameters: ['f32', 'anyfunc', 'i64']});
let tag9 = new WebAssembly.Tag({parameters: ['i64', 'anyfunc']});
let global1 = new WebAssembly.Global({value: 'f64', mutable: true}, 596401.8113845346);
let global2 = new WebAssembly.Global({value: 'f64', mutable: true}, 45872.9845351013);
let global3 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
let global4 = new WebAssembly.Global({value: 'i64', mutable: true}, 3913557490n);
let global5 = new WebAssembly.Global({value: 'i32', mutable: true}, 3038033284);
let global6 = new WebAssembly.Global({value: 'i32', mutable: true}, 2542291009);
let table0 = new WebAssembly.Table({initial: 93, element: 'externref'});
let table1 = new WebAssembly.Table({initial: 24, element: 'anyfunc', maximum: 256});
let table4 = new WebAssembly.Table({initial: 97, element: 'externref', maximum: 212});
let table5 = new WebAssembly.Table({initial: 78, element: 'anyfunc', maximum: 669});
let table6 = new WebAssembly.Table({initial: 18, element: 'anyfunc', maximum: 23});
let m0 = {fn0, global1, global2, global3, global5, table3: table0, table5, table6, tag8, tag9, tag10: tag7, tag13: tag7};
let m1 = {global4, global7: 3098420685, tag7, tag11: tag8, tag12: tag8};
let m2 = {global0: 42196913, global6, table0, table1, table2: table0, table4, table7: table0};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('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', importObject0);
let {fn1, fn2, global8, global9, global10, memory0, table8, table9, table10, table11, table12, table13, table14, table15, table16, table17, tag0, tag1, tag2, tag3, tag4, tag5, tag6} = /**
  @type {{
fn1: (a0: I64, a1: I32, a2: I32) => [I64, I32, I32],
fn2: () => I32,
global8: WebAssembly.Global,
global9: WebAssembly.Global,
global10: WebAssembly.Global,
memory0: WebAssembly.Memory,
table8: WebAssembly.Table,
table9: WebAssembly.Table,
table10: WebAssembly.Table,
table11: WebAssembly.Table,
table12: WebAssembly.Table,
table13: WebAssembly.Table,
table14: WebAssembly.Table,
table15: WebAssembly.Table,
table16: WebAssembly.Table,
table17: WebAssembly.Table,
tag0: WebAssembly.Tag,
tag1: WebAssembly.Tag,
tag2: WebAssembly.Tag,
tag3: WebAssembly.Tag,
tag4: WebAssembly.Tag,
tag5: WebAssembly.Tag,
tag6: WebAssembly.Tag
  }} */ (i0.instance.exports);
table8.set(61, table15);
table0.set(65, table10);
global8.value = 0;
global4.value = 0n;
global1.value = 0;
report('progress');
try {
  for (let k=0; k<25; k++) {
  let zzz = fn1(global4.value, k, global10.value);
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
  for (let k=0; k<26; k++) {
  let zzz = fn2();
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
  for (let k=0; k<23; k++) {
  let zzz = fn1(global4.value, global5.value, k);
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
  for (let k=0; k<24; k++) {
  let zzz = fn2();
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') {} else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) {} else { throw e; }
}
let tables = [table0, table4, table13, table10, table8, table16, table11, table14, table15, table1, table5, table6, table12, table17, table9];
for (let table of tables) {
for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
}
})().then(() => {
  report('after');
}).catch(e => {
  report('error');
})
