function test() {

var proxied = function(){};
var passed = false;
new new Proxy(proxied, {
  construct: function (t, args) {
    passed = t === proxied && args + "" === "foo,bar";
    return {};
  }
})("foo","bar");
return passed;
      
}

if (!test())
    throw new Error("Test failed");

