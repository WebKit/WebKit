// This test passes if it does not crash.

var regexp5 = new RegExp(/\t%%/);
Object.defineProperty(regexp5, "lastIndex" ,{value:"\w?\B", writable:false});
Object.seal(regexp5);
