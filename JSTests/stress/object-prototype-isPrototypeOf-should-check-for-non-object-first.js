//@ runDefault

var result;
var exception = undefined;
try {
    result = Object.prototype.isPrototypeOf.call(null);
} catch (e) {
    exception = e;
}

if (typeof exception != "undefined")
    throw "FAILED";
if (typeof result != "boolean")
    throw "FAILED";
if (result != false)
    throw "FAILED";
