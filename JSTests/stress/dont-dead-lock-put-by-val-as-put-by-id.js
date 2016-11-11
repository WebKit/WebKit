function ident() { return "foo"; }
noInline(ident);

let o = {
    set foo(x) {
        foo(false);
    }
};

function foo(cond) {
    if (cond)
        o[ident()] = 20;
}

for (let i = 0; i < 10000; i++) {
    foo(true);
}
