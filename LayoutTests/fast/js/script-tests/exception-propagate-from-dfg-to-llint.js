description("Ensures that we pass exceptions to the correct codeblock when throwing from the DFG to the LLInt.");
var o = {
    toString: function() { if (shouldThrow) throw {}; return ""; }
};

var shouldThrow = false;
function h(o) {
    return String(o);
}

try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}
try { shouldThrow = !shouldThrow; h(o); } catch (e) {}


function g() {
    with({})
        h(o);
}

function f1() {
    try {
        g();
    } catch (e) {
        testFailed("Caught exception in wrong codeblock");
    }
}

function f2() {
    try {
        g();
    } catch (e) {
        testPassed("Caught exception in correct codeblock");
    }
}

f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
f1();
shouldThrow = true;
f2();
var successfullyParsed = true;
