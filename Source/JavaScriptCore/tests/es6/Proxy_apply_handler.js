function test() {

var proxied = function(){};
var passed = false;
var host = {
  method: new Proxy(proxied, {
    apply: function (t, thisArg, args) {
      passed = t === proxied && thisArg === host && args + "" === "foo,bar";
    }
  })
};
host.method("foo", "bar");
return passed;
      
}

if (!test())
    throw new Error("Test failed");

