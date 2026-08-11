// Test that the ClonedArguments created by the Function.arguments will properly
// keep its callee alive.  This test should not crash and should not print any error
// messages.

var cachedArguments = [];
var numberOfEntries = 1000;

function makeTransientFunction(i) {
    function transientFunc() {
        cachedArguments[i] = transientFunc.arguments;
    }
    return transientFunc;
}

for (i = 0; i < numberOfEntries; i++) {
    var transientFunc = makeTransientFunction(i);
    transientFunc();
    // At this point, the only reference to the transient function is from
    // cachedArguments[i].callee.
}

gc();

// Allocate a bunch of memory to stomp over the transient functions that may have been
// erroneously collected. webkit.org/b/145709
for (i = 0; i < numberOfEntries; i++) {
    new Object();
}

for (i = 0; i < numberOfEntries; i++) {
    var callee = cachedArguments[i].callee;
    if (typeof callee != "function")
        print("ERROR: callee is " + callee); 
}
