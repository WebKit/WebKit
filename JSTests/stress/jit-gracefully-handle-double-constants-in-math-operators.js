function assert(b) {
    if (!b)
        throw new Error("bad assertion!");
}


function test() {
    let cases = [
        ["/", 1],
        ["*", 1],
        ["+", 2],
        ["-", 0],
        [">>", 0],
        [">>>", 0],
        ["<<", 2],
        ["^", 0],
        ["&", 1],
    ];

    for (let [op, result] of cases) {
        let program = `for (let i = 0; i < 500; i++) { assert((1,1)${op}1 === ${result}); }`;
        eval(program);
    }
}
test();
