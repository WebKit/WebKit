function test() {

var a = { foo: 1, bar: 2 };
a[Symbol.unscopables] = { bar: true };
with (a) {
  return foo === 1 && typeof bar === "undefined";
}
      
}

if (!test())
    throw new Error("Test failed");

