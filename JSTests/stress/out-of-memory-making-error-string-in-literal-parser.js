//@ skip if $memoryLimited
//@ slow!
//@ runDefault

var exception;
try {
    eval("JSON.parse(''.padStart(2 ** 31 - 1, 'a'))");
} catch (e) {
    exception = e;
}

if (exception != 'SyntaxError: JSON Parse error: Unexpected identifier "aaaaaaaaaa..."')
    throw "FAIL: actual " + exception;
