// See https://tc39.github.io/ecma262/#sec-object.freeze
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
    visited.push("before_freeze");
    Object.freeze(proxy);
    visited.push("after_freeze");
} catch (e) {
    visited.push("catch");
    exception = e;
}

var exceptionStr = "" + exception;
if (!exceptionStr.startsWith("TypeError:"))
    throw "Did not throw expected TypeError";

if (visited != "before_freeze,proxy_preventExtensions,catch")
    throw "ERROR: visited = " + visited;
