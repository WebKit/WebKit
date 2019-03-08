//@ runDefault("--forceEagerCompilation=1")

function foo() {
    let array = [];
    for (let a = 0; a < 4; a++) {
        array[a + 1] = 0;
    }
    gc();
    array.length=0;
    gc();
    var bar = 0;
    for (var i = 0; i < 1000; i++) {
        bar[0] = String.fromCharCode(i)[0];
    }
}

foo();
