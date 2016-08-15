function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);

function shouldThrowTDZ(func) {
    var hasThrown = false;
    try {
        func();
    } catch(e) {
        if (e.name.indexOf("ReferenceError") !== -1)
            hasThrown = true;
    }
    assert(hasThrown);
}
noInline(shouldThrowTDZ);




function foo() {
    return lexicalVariableNotYetDefined;
}

function bar() {
    lexicalVariableNotYetDefinedSecond = 300;
    return lexicalVariableNotYetDefinedSecond;
}
