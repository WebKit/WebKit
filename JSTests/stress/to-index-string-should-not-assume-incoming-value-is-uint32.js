//@ runDefault("--useRandomizingFuzzerAgent=1")

function foo() {
    for (var x in ['a', 'b']) {
        if (x === '') {
            break;
        }
    }
    return false && Object.prototype.hasOwnProperty
}

for (var i = 0; i < 10000; ++i)
    foo();
