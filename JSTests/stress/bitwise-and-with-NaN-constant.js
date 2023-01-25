function foo() {
    let x = 0;
    let y = ++x;
    let z = x - NaN;
    z &= y;
    if (z !== 0)
        throw 'Unexpected result of bitwise AND: ' + z;
}

for (let i = 0; i < 10000; ++i) {
    foo();
}
