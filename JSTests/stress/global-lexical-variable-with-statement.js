function assert(cond) {
    if (!cond)
        throw new Error("broke assertion");
}
noInline(assert);
function shouldThrowInvalidConstAssignment(f) {
    var threw = false;
    try {
        f();
    } catch(e) {
        if (e.name.indexOf("TypeError") !== -1 && e.message.indexOf("readonly") !== -1)
            threw = true;
    }
    assert(threw);
}
noInline(shouldThrowInvalidConstAssignment);


function makeObj() {
    return {foo: 20};
}
noInline(makeObj);

let foo = "foo";
const bar = "bar"; 

for (var i = 0; i < 100; i++) {
    with (makeObj()) {
        assert(foo === 20);
        assert(bar === "bar");
        shouldThrowInvalidConstAssignment(function() { bar = 20; });
    }
}
