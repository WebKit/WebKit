function foo() {
    constValue = 20;
}

function bar() {
    destructureObj = 100;
}

function baz() {
    destructureArr = 100;
}

for (var i = 0; i < 1000; i++) {
    shouldThrowInvalidConstAssignment(foo);
    assert(constValue === "const");

    shouldThrowInvalidConstAssignment(bar);
    assert(destructureObj === 20);

    shouldThrowInvalidConstAssignment(baz);
    assert(destructureArr === 40);
}
