//@ runDefault("--forceEagerCompilation=1", "--useConcurrentJIT=0")

function foo(a, a) {
    function x() {
        eval();
    }
}
foo();
foo();
foo();
foo();
foo();
foo(0);
