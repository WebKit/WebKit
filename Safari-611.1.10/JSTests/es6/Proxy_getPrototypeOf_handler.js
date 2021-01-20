function test() {

var proxied = {};
var fakeProto = {};
var proxy = new Proxy(proxied, {
  getPrototypeOf: function (t) {
    return t === proxied && fakeProto;
  }
});
return Object.getPrototypeOf(proxy) === fakeProto;
      
}

if (!test())
    throw new Error("Test failed");

