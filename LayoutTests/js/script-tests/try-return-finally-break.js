description(
"Tests what would happen if you have a break in the finally block. The correct outcome is for this test to not crash during bytecompilation."
);

function foo() {
    do {
        do {} while (false);

        try {
            do {
                return null;
            } while (false);
        } finally {
            break;
        }
    } while (false);
}

foo();
testPassed("It worked.");

