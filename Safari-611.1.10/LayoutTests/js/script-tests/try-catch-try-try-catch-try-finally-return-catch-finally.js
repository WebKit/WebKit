description(
"Tests what would happen if you a throwing operation at the beginning of a finally blow that gets inlined inside a complicated catch/finally stack. The correct outcome is for this test to not crash during exception throwing."
);

function foo() {
    try{
        N
    } catch(x) {
        try {
            try {
                w
            } catch(x) {
                try {
                } finally {
                    return
                }
            }
        } catch (a) {
        }
    } finally {
        z
    }
}

try {
    foo();
} catch (e) {
    testPassed("It worked.");
}

