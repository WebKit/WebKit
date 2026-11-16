//@ runBigIntEnabled

function assert(a, e) {
    if (a !== e)
        throw new Error("Bad!");
}

function foo(o) {
    let c;
    do {
    
        let a = 2 * o;
        o.bigInt = true;
        let b = 1n * o;
        for (let i = 0; i < 10000; i++) {
            c = i;
        }

        let d = b * o;
        c = a * d;
    } while(false);
}
noInline(foo);

let o = {
    valueOf: function () {
        return this.bigInt ? 2n : 2;
    }
}

for (let i = 0; i < 1000; i++) {
    try {
        o.bigInt = false;
        foo(o);
    } catch(e) {
        assert(e instanceof TypeError, true);
    }
}

