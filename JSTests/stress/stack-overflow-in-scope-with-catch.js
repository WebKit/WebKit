//@ skip if $memoryLimited

// This tests can easily use more than the 600M that $memoryLimited devices
// are capped at due JSCTEST_memoryLimit. Skip it to avoid the crash as a
// result of exceeding that limit.

let count = 0;

function foo() {
    if (count++ > 1e5)
        return;
    for (let j = 0; j < 5; j++) {
        try {
            foo();
        } catch (e) {
            Set[Symbol.hasInstance] = function() { };
            foo();
        } finally {
            function bar() { }
        }

        function goo() {
            function baz() { }
            baz();
            baz(bar);
        }
    }
}
foo();
