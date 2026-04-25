// This test should not crash.

Object.defineProperty(Object.prototype, 0, { set() {} });

var foo = Function.bind.call(new Proxy(Array, {}));
for (var i = 10; i < 50; ++i) {
    var args = Array(i).fill(i);
    new foo(...args);
    gc()
}
