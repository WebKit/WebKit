//@skip
// TODO: unskip after rdar://103455312 ([wasm] —watchdog should work for WebAssembly code)
//@runDefault("--maxPerThreadStackUsage=1572864", "--useConcurrentJIT=0")
let tools;
if (globalThis.callerIsBBQOrOMGCompiled) {
  function instantiateJsc(filename, importObject) {
    let bytes = read(filename, 'binary');
    return WebAssembly.instantiate(bytes, importObject, 'x');
  }
  const log = function() {};
  const report = $.agent.report;
  const isJIT = callerIsBBQOrOMGCompiled;
  tools = {log, report, isJIT, instantiate: instantiateJsc};
} else {
  function instantiateBrowser(filename, importObject) {
    let bytes = fetch(filename);
    return WebAssembly.instantiateStreaming(bytes, importObject, 'x');
  }
  const log = function() {};
  const report = function() {};
  const isJIT = () => 1;
  tools = {log, report, isJIT, instantiate: instantiateBrowser};
}
const {log, report, isJIT, instantiate} = tools;
const extra = {isJIT};
(async function () {
let tag3 = new WebAssembly.Tag({parameters: []});
let tag5 = new WebAssembly.Tag({parameters: ['anyfunc']});
let tag10 = new WebAssembly.Tag({parameters: []});
let global0 = new WebAssembly.Global({value: 'i64', mutable: true}, 199896032n);
let global1 = new WebAssembly.Global({value: 'externref', mutable: true}, {});
let global4 = new WebAssembly.Global({value: 'i64', mutable: true}, 161444932n);
let m2 = {global2: new WebAssembly.Global({value: 'externref', mutable: false}, {}), global4, global5: 623044.865719049, tag3, tag7: tag3, tag8: tag3, tag10};
let m0 = {global0, global3: global1, tag4: tag3, tag11: tag5};
let m1 = {global1, tag5, tag6: tag3, tag9: tag3};
let importObject0 = /** @type {Imports2} */ ({extra, m0, m1, m2});
let i0 = await instantiate('omg-stack-overflow.wasm', importObject0);
let {fn0, global6, global7, global8, memory0, table0, table1, tag0, tag1, tag2} = /**
  @type {{
fn0: () => I32,
global6: WebAssembly.Global,
global7: WebAssembly.Global,
global8: WebAssembly.Global,
memory0: WebAssembly.Memory,
table0: WebAssembly.Table,
table1: WebAssembly.Table,
tag0: WebAssembly.Tag,
tag1: WebAssembly.Tag,
tag2: WebAssembly.Tag
  }} */ (i0.instance.exports);
global8.value = 0;
global1.value = 'a';
log('calling fn0');
report('progress');
try {
  for (let k=0; k<12; k++) {
  let zzz = fn0();
  zzz?.toString();
  }
} catch (e) {
  if (e instanceof WebAssembly.Exception) {
  log(e); if (e.stack) { log(e.stack); }
  } else if (e instanceof TypeError) {
  if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
  } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
}
// log('calling fn0');
// report('progress');
// try {
//   for (let k=0; k<22; k++) {
//   let zzz = fn0();
//   zzz?.toString();
//   }
// } catch (e) {
//   if (e instanceof WebAssembly.Exception) {
//   log(e); if (e.stack) { log(e.stack); }
//   } else if (e instanceof TypeError) {
//   if (e.message === 'an exported wasm function cannot contain a v128 parameter or return value') { log(e); } else { throw e; }
//   } else if (e instanceof WebAssembly.RuntimeError || e instanceof RangeError) { log(e); } else { throw e; }
// }
// let tables = [table1, table0];
// for (let table of tables) {
// for (let k=0; k < table.length; k++) { table.get(k)?.toString(); }
// }
})().then(() => {
  log('after')
  report('after');
}).catch(e => {
  log(e)
  log('error')
  report('error');
})
