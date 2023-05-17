//@skip if $memoryLimited
//@ runDefault
let f64 = new Float64Array(2 ** 29);
f64.__proto__ = new Uint8Array();
let x = f64.subarray();
f64.set(x);
