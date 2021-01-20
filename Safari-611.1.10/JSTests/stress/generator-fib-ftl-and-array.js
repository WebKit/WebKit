(function () {
    function *fib()
    {
        let a = 1;
        let b = 1;
        let c = [ 0 ];
        while (true) {
            c[0] = a;
            yield c;
            [a, b] = [b, a + b];
        }
    }

    let value = 0;
    for (let i = 0; i < 1e4; ++i) {
        let f = fib();
        for (let i = 0; i < 100; ++i) {
            value = f.next().value;
        }
        if (value[0] !== 354224848179262000000)
            throw new Error(`bad value:${result}`);
    }
}());
