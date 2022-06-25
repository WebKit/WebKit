function foo() {
    let a0 = ['a', {}];
    for (let q in a0) {
        a0 = new Uint8Array();
    }
}

for (let i=0; i<1e3; i++) {
    foo();
}
