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
