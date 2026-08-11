let p = { get: undefined, set: undefined, };

function foo() {
    let o = {};
    for (let i = 0; i < 100; i++) {
        for (let j = 0; j < 5; j++)
            delete o.x;
        o.x;
        Object.defineProperty(o, 'x', p);
    }
}

foo()
