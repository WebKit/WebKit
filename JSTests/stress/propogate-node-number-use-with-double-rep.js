function test(opt) {
    let expected = opt();
    for (let i = 0; i < 1e3; i++) {
        let actual = opt();
        if (actual != expected)
            throw new Error("bad actual " + actual + " expected " + expected);
    }
}

{
    function opt1(f) {
        var x = -19278.05 >>> NaN;
        var y = -19278.05 + x;
        var z = y >> 0;
        return z;
    }
    noInline(opt1);

    function opt2(v) {
        let x = -1 >>> v;
        let y = x + -0.2;
        let z = y | 0;
        return z;
    }
    noInline(opt2);

    test(opt1);
    test(opt2);
}
