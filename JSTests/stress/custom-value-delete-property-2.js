const Value = $vm.createCustomTestGetterSetter();
function test2() {
    function foo() {
        const o = {};
        for (let i = 0; i < 8; i++) {
            let x = o.customValue2;
            o.__proto__ = Value;
        }
        delete Value.customValue;
    }

    foo();
    foo();
}
test2();
