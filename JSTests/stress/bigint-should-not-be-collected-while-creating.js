//@ runDefault("--collectContinuously=1")
function foo(a0) {
    if (a0 == 0) {
        return
    }

    let increment = 10000n

    function bar() {
        for (let i = 0n; i < 3000000000n; i = i + increment);
    }

    bar();
}

foo(0);
foo(1);
