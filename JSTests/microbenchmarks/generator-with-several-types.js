(function () {
    function *g1() {
        for (let i = 0; i < 100; ++i)
            yield i;
    }

    function *g2() {
        for (let i = 0; i < 100; ++i)
            yield String.fromCharCode(i);
    }

    function *g3() {
        for (let i = 0; i < 100; ++i)
            yield {};
    }

    for (let i = 0; i < 1e4; ++i) {
        {
            let g = g1();
            for (let element of g);
        }

        {
            let g = g2();
            for (let element of g);
        }

        {
            let g = g3();
            for (let element of g);
        }
    }
}());
