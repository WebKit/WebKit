(function() {
    function test(a, b) {
        return String.fromCharCode(a, b);
    }
    noInline(test);

    let result;
    for (let i = 0; i < 1e6; ++i)
        result = test(i & 0x7f, 0x41);
    if (result !== "?A")
        throw new Error("bad result: " + result);
})();
