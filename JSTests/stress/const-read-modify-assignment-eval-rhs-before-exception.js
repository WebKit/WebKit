(function() {
    let count = 0;

    const x = 42;

    try {
        x += (() => ++count)();
    } catch { }

    if (count !== 1)
        throw new Error(`expected 1 but got ${count}`);
})();

(function () {
    let count = 0;

    const x = 42;
    function captureX() { return x; }

    try {
        x += (() => ++count)();
    } catch { }

    if (count !== 1)
        throw new Error(`expected 1 but got ${count}`);
})();
