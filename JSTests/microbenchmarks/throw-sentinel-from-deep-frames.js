(function() {
    const sentinel = { code: 42 };
    function thrower(n) { if (n === 0) throw sentinel; return thrower(n - 1); }

    let r = 0;
    for (let i = 0; i < 100000; i++) {
        try { thrower(40); } catch (e) { r = e.code; }
    }

    if (r !== 42)
        throw new Error("Bad value!");
})();
