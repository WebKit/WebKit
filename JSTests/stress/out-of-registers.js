function opt(a1, a2) {
    try {
        undefined(a1);
    } catch(e) {}
    ([1, 2, 3]).copyWithin();
    function test() {
        "test" + a2;
    }
    for (let i = 0; i < 100; i++) {
        test("source");
    }
    for (let j = 0; j < 100; j++) {
        try {
            undefined.asin();
        } catch(e) {}
        for (let k = 0; k < 100; k++) {}
    }
}
for (let m = 0; m < 100; m++) {
    opt(m, opt);
}
