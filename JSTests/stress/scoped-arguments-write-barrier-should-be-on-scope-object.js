//@ runDefault
// This test should not crash.

var arr = [];
let numberOfIterations = 1000;

function captureScopedArguments(i) {
    try {
        eval("arr[" + i + "] = arguments");
    } catch(e) {
    }
}

function addPointersToEdenGenObjects(i) {
    Array.prototype.push.call(arr[i], [,,]);

    try {
        Array.prototype.reverse.call(arr[i])
    } catch (e) {
    }
}

for (var i = 0; i < numberOfIterations; i++) {
    captureScopedArguments(i);
}

gc(); // Promote those ScopeArguments to the old generation.

for (var i = 0; i < numberOfIterations; i++) {
    addPointersToEdenGenObjects(i);
}

edenGC(); // Do eden GC to scan the remembered set which should include the ScopedArguments.

gc(); // Scan the ScopedArguments again. They better not point to collected objects.
