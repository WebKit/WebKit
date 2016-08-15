function test() {

var proxied = {};
var fakeDesc = { value: "foo", configurable: true };
var returnedDesc = Object.getOwnPropertyDescriptor(
  new Proxy(proxied, {
    getOwnPropertyDescriptor: function (t, k) {
      return t === proxied && k === "foo" && fakeDesc;
    }
  }),
  "foo"
);
return (returnedDesc.value     === fakeDesc.value
  && returnedDesc.configurable === fakeDesc.configurable
  && returnedDesc.writable     === false
  && returnedDesc.enumerable   === false);
      
}

if (!test())
    throw new Error("Test failed");

