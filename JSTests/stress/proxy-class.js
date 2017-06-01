function assert(b) {
    if (!b)
        throw new Error("Bad assertion");
}

{
    const value = 'foo-bar';

    class SuperClass {
        constructor() {
            this._id = value;
        }
    }

    let ProxiedSuperClass = new Proxy(SuperClass, {});

    for (let i = 0; i < 500; i++) {
        let p = new ProxiedSuperClass;
        assert(p._id === value);
    }

    class A extends ProxiedSuperClass {
        constructor() {
            super();
        }
    }

    assert(A.__proto__ === ProxiedSuperClass);

    for (let i = 0; i < 500; i++) {
        let a = new A;
        assert(a._id === value);
    }
}