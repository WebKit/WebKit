class Foo extends Object {
    constructor(c1, c2) {
        if (c1)
            super();
        let arrow = () => {
            if (c2)
                this.foo = 20;
            else
                this.foo = 40;
        };
        noInline(arrow);
        arrow();
    }
}
noInline(Foo);

for (let i = 0; i < 1000; ++i)
    new Foo(true, !!(i%2));

let threw = false;
try {
    new Foo(false, true);
} catch {
    threw = true;
} finally {
    if (!threw)
        throw new Error("Bad")
}
