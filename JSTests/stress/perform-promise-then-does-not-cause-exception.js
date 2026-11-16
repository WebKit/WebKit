try {
function fn0() {}
var v0 = new Promise(function () {});
function fn3() {}
try {
  v0.constructor = fn3;
} catch (e) {}
fn0(Promise.resolve(v0) instanceof Promise);
var v4 = __c_0.resolve().then();
// Flags:
} catch { }
