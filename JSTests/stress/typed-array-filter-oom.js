//@ skip if $memoryLimited
this.g ??= createGlobalObject();
function f1(a2) {
  function f3() {
    Object.groupBy.__proto__ = Array.prototype.splice;
    Object.groupBy.__defineGetter__('__proto__', () => {
      delete Object.groupBy[0];
    }), f3[2] = 'f'.lastIndexOf(a2);
    return arguments;
  }
  f3[2] = a2;
  return f3();
}
try {
    const v9 = new this.g.Int32Array(1073741824);
    v9['filter'](f1);
    gc();
    String.prototype.blink.__proto__ = v9;
} catch { }
