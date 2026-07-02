//@ slow!
//@ memoryHog!
//@ runDefault

var longString = "x".repeat(2 ** 30);
var arr = Array(4).fill(longString);

var exception;
try {
    JSON.stringify(arr);
} catch (e) {
    exception = e;
}

if (exception != "RangeError: Out of memory")
    throw "FAILED";
