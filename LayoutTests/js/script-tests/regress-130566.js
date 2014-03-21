description(
"This tests ensures that the DFG StackLayoutPhase is only accessing a union'ed calleeVariable. This test should not crash."
);

// Regression test for <https://webkit.org/b/130566>.
function test()
{
    function doTest() {
        (function foo(a) {
            if (a > 0) {
                foo(a  - 1);
            }
        }) (424);
    }

    for (var i = 0; i < 1000; i++) {
        try {
            doTest();
        } catch(runError) {
        }
    }
}

test();
