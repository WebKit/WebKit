function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);

function bar(...args) {
    return args;
}

function foo(a, ...args) {
    try {
        let r = bar(...args, ...args);
        return r;
    } catch(e) {
        return a;
    }
}
noInline(foo);

for (let i = 0; i < 10000; i++) {
    let args = [];
    for (let i = 0; i < 400; i++) {
        args.push(i);
    }

    let o = {};
    let r = foo(o, ...args);
    let i = 0;
    for (let arg of args) {
        assert(r[i] === arg);
        i++;
    }
    for (let arg of args) {
        assert(r[i] === arg);
        i++;
    }
}

for (let i = 0; i < 20; i++) {
    let threw = false;
    let o = {};
    let args = [];
    let argCount = maxArguments() * (2/3);
    argCount = argCount | 0;
    for (let i = 0; i < argCount; i++) {
        args.push(i);
    }

    let r = foo(o, ...args);
    assert(r === o);
}
