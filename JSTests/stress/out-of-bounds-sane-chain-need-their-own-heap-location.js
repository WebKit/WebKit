const a0 = [0];
function foo() {
    for (let i=1; i<100; i++) {
        a0[i];
        a0[i] = undefined;
        let x = [];
        for (let j = 0; j < 20; j++) {}
    }
}

for (let i=0; i<10000; i++) {
    foo();
}
