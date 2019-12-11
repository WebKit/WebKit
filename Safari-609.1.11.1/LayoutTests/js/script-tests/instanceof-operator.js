description(
"instanceof test"
);

getterCalled = false;
try {
    ({} instanceof { get prototype(){ getterCalled = true; } });
} catch (e) {
}
shouldBeFalse("getterCalled");

// Regression test for <https://webkit.org/b/129768>.
// This test should not crash.
function dummyFunction() {}
var c = dummyFunction.bind();

function foo() {
    // To reproduce the issue of <https://webkit.org/b/129768>, we need to do
    // an instanceof test against an object that has the following attributes:
    // ImplementsHasInstance, and OverridesHasInstance.  A bound function fits
    // the bill.
    var result = c instanceof c;

    // This is where the op_check_has_instance bytecode jumps to after the
    // instanceof test. At this location, we need the word at offset 1 to be
    // a ridiculously large value that can't be a valid stack register index.
    // To achieve that, we use an op_loop_hint followed by any other bytecode
    // instruction. The op_loop_hint takes up exactly 1 word, and the word at
    // offset 1 that follows after is the opcode of the next instruction.  In
    // the LLINT, that opcode value will be a pointer to the opcode handler
    // which will be large and exactly what we need.  Hence, we plant a loop
    // here for the op_loop_hint, and have some instruction inside the loop.
    while (true) {
        var dummy2 = 123456789;
        break;
    }
    return result;
}

shouldBeFalse("foo()");

