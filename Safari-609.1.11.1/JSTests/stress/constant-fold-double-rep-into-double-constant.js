function bar(o) {
    for (let i = 0; i < 2; i++)
        o[i] = undefined;
    o.length = undefined;
    return o;
}

function foo(a) {
    bar(a);
    undefined + bar(0) + bar(0);
    for(let i = 0; i < 10000000; i++) {}
}

foo({});
