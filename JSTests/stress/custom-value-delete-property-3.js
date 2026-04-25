function test6() {
    var custom = $vm.createCustomTestGetterSetter();
    function foo() {
        const o = {};
        for (let i = 0; i < 8; i++) {
            o.customValue2;
            o.__proto__ = custom;
        }
        delete custom.customValue2;
    }

    foo();
    foo();
}
test6();
