for (let i = 0; i < 1000; i++) {
}
for (let i = 0; i < 10; ++i) {
    function foo() {
        for (let j = 0; j < 3; j = j + "asdf") {
            const cond = Error != Error;
            if (!cond) {
                42[0];
            }

            function bar(arg) {
                return arg.baz = 42;
            }
            for (let k = 0; k < 10000; ++k) {
                bar({}, ...arguments);
            }
        }
        for (let j = 0; j < 1000000; ++j) {}
    }
    foo();
}

