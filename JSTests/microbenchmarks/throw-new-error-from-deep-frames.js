(function() {
    function thrower(n) {
        if (n === 0) {
            const e = new Error("boom");
            e.code = 42;
            throw e;
        }
        return thrower(n - 1);
    }

    let r = 0;
    for (let i = 0; i < 50000; i++) {
        try { thrower(40); } catch (e) { r = e.code; }
    }

    if (r !== 42)
        throw new Error("Bad value!");
})();
