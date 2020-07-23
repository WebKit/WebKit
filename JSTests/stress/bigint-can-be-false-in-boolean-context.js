function foo() {
    let b0 = 0n.valueOf();
    function bar() {
        let b1 = b0;
        if (!b0) {
            b1[Symbol.for('a')];
        }
    }
    bar();
}

for (let i=0; i<10000; i++) {
    foo();
}
