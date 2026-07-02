let objs = new Array(1000);
for (let i of objs.keys()) {
    let o = {};
    // Make the object an uncacheable dictionary.
    o.foo = 1;
    delete o.foo;
    objs[i] = o;
}

function f(o) {
    o.foo = 42;
}

for (let obj of objs) {
    let setter = new Function(`
        Object.defineProperty(this, "foo", {
            writable: true,
            configurable: true,
            value: null
        });
        let o = Object.create(this);
        // Need eval to get a new IC to flatten obj.
        let str = "for (let i = 0; i < 1000; i++) o.foo";
        eval(str);
    `);

    obj.__defineSetter__("foo", setter);
    f(obj);
    f(obj);
}
