//@ slow!
//@ skip if $memoryLimited
//@ skip if $architecture != "arm64" and $architecture != "x86-64"
//@ runDefault if !$memoryLimited

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
