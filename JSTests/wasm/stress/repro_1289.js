function debuggingHelper(x) {
  // print(x)
  // $vm.dumpStack();
}



let tools;
{
  function instantiateJsc(filename, importObject) {
    let bytes = read(filename, 'binary');
    return WebAssembly.instantiate(bytes, importObject, 'x');
  }
  const log = debug;
  const report = $.agent.report;
  const isJIT = callerIsBBQOrOMGCompiled;
  tools = {log, report, isJIT, instantiate: instantiateJsc};
}
const {log, report, isJIT, instantiate} = tools;
const extra = {isJIT};
(async function () {
let memory0 = new WebAssembly.Memory({initial: 3510, shared: false, maximum: 4275});
let fn0 = function () {
};
let fn1 = function () {
};
let fn2 = function () {
};
let tag2 = new WebAssembly.Tag({parameters: []});
let tag8 = new WebAssembly.Tag({parameters: []});
let global0 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, null);
let global1 = new WebAssembly.Global({value: 'i64', mutable: true}, 2687521653n);
let global2 = new WebAssembly.Global({value: 'i64', mutable: true}, 3499115411n);
let global3 = new WebAssembly.Global({value: 'f64', mutable: true}, 46546.77541190513);
let global4 = new WebAssembly.Global({value: 'i32', mutable: true}, 3310225489);
let global5 = new WebAssembly.Global({value: 'i64', mutable: true}, 291230721n);
let global7 = new WebAssembly.Global({value: 'i64', mutable: true}, 2617080151n);
let global8 = new WebAssembly.Global({value: 'anyfunc', mutable: true}, global0.value);
let m2 = {fn1, fn2, global0, memory0, tag3: tag2, tag4: tag2, tag6: tag2};
let m0 = {fn0, global1, global3, global4, global6: global3, tag2, tag7: tag2, tag10: tag2};
let m1 = {global2, global5, global7, global8, tag5: tag2, tag8, tag9: tag2, debuggingHelper};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('repro_1289.wasm', importObject0);
let {fn3} =  (i0.instance.exports);
// log('calling fn3');
report('progress');
  for (let k=0; k<24; k++) {
    let zzz = fn3();
    if (!(zzz instanceof Array)) { throw new Error('expected array but return value is '+zzz); }
    if (zzz.length != 21) { throw new Error('expected array of length 21 but return value is '+zzz); }
    let [r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15, r16, r17, r18, r19, r20] = zzz;
    r18?.toString();
  }
})().then(() => {
  // log('after')
  report('after');
}).catch(e => {
  report('after');
})
