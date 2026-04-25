(function() {
    const iterations = 2000000;

    let re1 = /\2(a)(b)/;
    let re2 = /\k<y>(?<x>a)(?<y>b)/;
    for (let i = 0; i < iterations; ++i) {
        re1.test("ab");
        re1.test("xx");
        re2.test("ab");
        re2.test("xx");
    }
})();
