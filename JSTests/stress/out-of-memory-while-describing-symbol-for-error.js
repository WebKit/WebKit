//@ skip if $memoryLimited

let a = "?".repeat(2147483647);
var exception;
try {
    var s = Symbol(a);
    new s(2);
} catch (e) {
    exception = e;
}

if (exception != "TypeError: Symbol is not a constructor (evaluating 'new s(2)')")
    throw "FAILED";

