// FIXME: unskip when this is solved
// https://bugs.webkit.org/show_bug.cgi?id=191163
//@ skip if $architecture == "arm"
//@ skip if $architecture == "x86"
function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}
noInline(assert);

function identity(a) { return a; }
noInline(identity);

function bar(...args) {
    return args;
}
noInline(bar);

function foo(a, ...args) {
    let arg = identity(a);
    try {
        let r = bar(...args, ...args);
        return r;
    } catch(e) {
        return arg;
    }
}
noInline(foo);

for (let i = 0; i < 40000; i++) {
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
