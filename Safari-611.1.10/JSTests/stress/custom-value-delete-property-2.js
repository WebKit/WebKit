function test2() {
    function foo() {
        const o = {};
        for (let i = 0; i < 8; i++) {
            let x = o.multiline;
            o.__proto__ = RegExp;
        }
        delete RegExp.input;
    }

    foo();
    foo();
}
test2();
