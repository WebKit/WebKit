// This regression test checks that a let in the TDZ state is handled properly
// with a for-of in a try as well as the ensuing catch block.

function test() {
    try {
        for ({o} of [, 0])
            ;
    } catch (e) {
        o[0] = 1.5;
    }
    let o = {
    };
}

for (i = 0; i < 1000; i++) {
    try {
        test();
    } catch(e) {
        if (e != "ReferenceError: Cannot access uninitialized variable.")
            throw "Expected \"ReferenceError: Cannot access uninitialized variable.\", but got \"" + e +"\"";
    }
}
