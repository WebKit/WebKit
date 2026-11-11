(function() {
    function test(lead, trail) {
        return String.fromCharCode(lead, trail);
    }
    noInline(test);

    let result;
    for (let i = 0; i < 1e6; ++i)
        result = test(0xd83d, 0xde00 + (i & 63));
    if (result !== "\u{1f63f}")
        throw new Error("bad result: " + result);
})();
