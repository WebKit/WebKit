let objs = [
    {
        __proto__: { 
            foo: 25
        },
        bar: 50,
        baz: 75,
        jaz: 80,
    },
    {
        __proto__: { 
            bar: 25
        },
        baz: 75,
        kaz: 80,
        bar: 50,
        jaz: 80,
    },
    {
        __proto__: { 
            bar: 25,
            jaz: 50
        },
        bar: 50,
        baz: 75,
        kaz: 80,
        jaz: 80,
        foo: 55
    }
];

function foo(o) {
    for (let p in o)
        o.hasOwnProperty(p);

}
noInline(foo);

let start = Date.now();
for (let i = 0; i < 1000000; ++i) {
    foo(objs[i % objs.length]);
}
const verbose = false;
if (verbose)
    print(Date.now() - start);
