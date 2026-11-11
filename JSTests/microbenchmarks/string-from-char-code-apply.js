(function() {
    function test(codes) {
        return String.fromCharCode.apply(null, codes);
    }
    noInline(test);

    let codes = [];
    let expected = "";
    for (let i = 0; i < 1024; ++i) {
        codes.push(32 + i % 90);
        expected += String.fromCharCode(32 + i % 90);
    }

    let result;
    for (let i = 0; i < 1e4; ++i)
        result = test(codes);
    if (result !== expected)
        throw new Error("bad result: " + result);
})();
