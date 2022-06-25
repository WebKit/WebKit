//@ runDefault("--useRandomizingFuzzerAgent=1", "--validateAbstractInterpreterState=1", "--jitPolicyScale=0", "--useConcurrentJIT=0")

function foo() {
    for (let i = 0; i < 3; i++) {
        const o = {};
        function bar(a0) {
            let x;
            do {
                function f() { z; }
                x = o;
                const y = typeof x === a0;
                [a0, 0.1];
                const z = 0 + y;
                const c = z % 1.0 + 0;
            } while (!x);
        }
        bar(0);
    }
}

foo();
