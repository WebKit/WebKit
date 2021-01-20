// This test should not crash.
var proto = Array.prototype;
class Test extends Array {}
new Test( 8, 9);

Object.defineProperty(proto, 324800, { });
new Test( 8, 9);
