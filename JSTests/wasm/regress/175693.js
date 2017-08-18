const file = "175693.wasm";

if (typeof console === 'undefined') {
  console = { log: print };
}
var binary;
if (typeof process === 'object' && typeof require === 'function' /* node.js detection */) {
  var args = process.argv.slice(2);
  binary = require('fs').readFileSync(file);
  if (!binary.buffer) binary = new Uint8Array(binary);
} else {
  var args;
  if (typeof scriptArgs != 'undefined') {
    args = scriptArgs;
  } else if (typeof arguments != 'undefined') {
    args = arguments;
  }
  if (typeof readbuffer === 'function') {
    binary = new Uint8Array(readbuffer(file));
  } else {
    binary = read(file, 'binary');
  }
}
var instance = new WebAssembly.Instance(new WebAssembly.Module(binary), {});
if (instance.exports.hangLimitInitializer) instance.exports.hangLimitInitializer();
try {
  console.log('calling: func_0');
instance.exports.func_0();
} catch (e) {
  console.log('   exception: ' + e);
}
if (instance.exports.hangLimitInitializer) instance.exports.hangLimitInitializer();
try {
  console.log('calling: hangLimitInitializer');
instance.exports.hangLimitInitializer();
} catch (e) {
  console.log('   exception: ' + e);
}
console.log('done.')
