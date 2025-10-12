
//@ runDefault("--forceEagerCompilation=true")
function foo() {
    const pattern = /o/gium;

    let ret = pattern.exec("function");

    const a = [];
    const b = a + -1;

    switch ("_") {
        case b:
            throw 1;
        default:
    }

    ret = pattern.exec("function");
    return ret;
}

let expected = foo();

let actual;
for (let i = 0; i < 10; i++) {
    actual = foo();
}
if (expected != actual)
    throw new Error("bad")
