function test() {

var proxied = { };
var passed = false;
var proxy = new Proxy(proxied, {
  set: function (t, k, v, r) {
    passed = t === proxied && k + v === "foobar" && r === proxy;
  }
});
proxy.foo = "bar";
return passed;
      
}

if (!test())
    throw new Error("Test failed");

