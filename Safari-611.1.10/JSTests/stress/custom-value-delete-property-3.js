function test6() {
    function foo() {
        const o = {};
        for (let i = 0; i < 8; i++) {
            o.lastMatch;
            o.__proto__ = RegExp;
        }
        delete RegExp.input;
    }

    foo();
    foo();
}
test6();
