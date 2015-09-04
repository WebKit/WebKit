function test() {

var proxied = { };
var proxy = new Proxy(proxied, {
  get: function (t, k, r) {
    return t === proxied && k === "foo" && r === proxy && 5;
  }
});
return proxy.foo === 5;
      
}

if (!test())
    throw new Error("Test failed");

