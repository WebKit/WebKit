//@ skip if $memoryLimited
function assert(a, message) {
    if (!a)
        throw new Error(message);
}

try {
    var foo = 'yy?x\uFFFD$w    5?\uFFFDo\uFFFD?\uFFFD\'i?\uFFFDE-N\uFFFD\uFFFD6_\uFFFD\\ d';
    foo = foo.padEnd(2147483644, 1);
    eval('foo()');
    assert(false, `Should throw OOM error`);
} catch (error) {
    assert(error.message == "Out of memory", "Expected OutOfMemoryError, but got: " + error);
}
