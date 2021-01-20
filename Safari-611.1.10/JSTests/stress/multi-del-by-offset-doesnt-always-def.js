let p = { get: undefined, set: undefined, };
let o1 = {};
Object.defineProperty(o1, 'x', p);

function foo(o) {
    if (!(delete o.x))
        o.x;
}
noInline(foo);

for (let i = 0; i < 10000; ++i) {
    foo(o1);
    foo({x : 42});
}
