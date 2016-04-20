function opaqueAdd(a)
{
    return a + 42;
}
noInline(opaqueAdd);

// Warm up.
for (let i = 0; i < 1e4; ++i) {
    let result = opaqueAdd(5);
    if (result !== 47)
        throw "Invalid opaqueAdd(5) at i = " + i;
}

// Overflow.
for (let i = 0; i < 1e3; ++i) {
    for (let j = -50; j < 50; ++j) {
        let result = opaqueAdd(2147483647 + j);
        if (result !== 2147483689 + j)
            throw "Invalid opaqueAdd(" + 2147483647 + j + ") at i = " + i + " j = " + j;
    }
}
