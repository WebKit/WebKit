var result = (function () {
    function *fib()
    {
        let a = 1;
        let b = 1;
        while (true) {
            yield a;
            [a, b] = [b, a + b];
        }
    }

    let value = 0;
    for (let i = 0; i < 1e4; ++i) {
        let f = fib();
        for (let i = 0; i < 100; ++i) {
            value = f.next().value;
        }
    }
    return value;
}());
if (result !== 354224848179262000000)
    throw new Error(`bad value:${result}`);
