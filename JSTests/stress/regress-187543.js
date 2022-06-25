// This test should not crash.

Object.defineProperty(Object.prototype, 0, { set() { } });
var foo = Function.bind.call(new Proxy(Array, {}));
gc();
new foo(0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23, 24, 25);
gc();
