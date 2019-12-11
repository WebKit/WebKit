//@ runDefault("--validateGraphAtEachPhase=1")

function f1() {
}

function f2() {
}

const a = [0];

function foo() {
    f1(...a);
    for (let i = 0; i < 2; i++) {
        f2([] > 0);
    }
}

for (var i = 0; i < 1000000; ++i) {
    foo();
}
