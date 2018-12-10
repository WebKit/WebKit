//@ runBigIntEnabled

function assert(a, e) {
    if (a !== e)
        throw new Error("Bad!");
}

function foo() {
    let c;
    do {
    
        let a = 2;
        let b = 3n;
        for (let i = 0; i < 10000; i++) {
            c = i;
        }

        c = a * b; 
    } while(true);

    return c;
}

try {
    foo();
} catch(e) {
    assert(e instanceof TypeError, true);
}

