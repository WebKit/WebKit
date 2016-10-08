// See https://tc39.github.io/ecma262/#sec-object.seal
// See https://tc39.github.io/ecma262/#sec-setintegritylevel

var x = [10];
var visited = [];

var proxy = new Proxy(x, {
    preventExtensions() {
        visited.push("proxy_preventExtensions");
        return false;
    }
});

var exception;
try  {
    visited.push("before_seal");
    Object.seal(proxy);
    visited.push("after_seal");
} catch (e) {
    visited.push("catch");
    exception = e;
}

var exceptionStr = "" + exception;
if (!exceptionStr.startsWith("TypeError:"))
    throw "Did not throw expected TypeError";

if (visited != "before_seal,proxy_preventExtensions,catch")
    throw "ERROR: visited = " + visited;
