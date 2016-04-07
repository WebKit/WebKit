class x {}

class Foo {}
class Foo extends Bar {}

class Foo {
    constructor()
    {
        1
    }
}

class Foo {
    constructor()
    {
        1
    }
    static staticMethod()
    {
        1
    }
}

class Foo {
    static staticMethod()
    {
        1
    }
}

class Foo extends Bar {
    constructor()
    {
        super()
    }
}

class Foo extends Bar {
    constructor()
    {
        1
    }
    static staticMethod()
    {
        super.staticMethod()
    }
}

class Foo extends Bar {
    constructor()
    {
        1
    }
    static staticMethod()
    {
        super.staticMethod()
    }
    method1()
    {
        1
    }
    method2(name, {options})
    {
        1
    }
    get getter()
    {
        1
    }
    set setter(x)
    {
        1
    }
    *gen1()
    {
        1
    }
    *gen2()
    {
        1
    }
}

A = class Foo extends (b ? Bar : Bar) {
    constructor()
    {
        new.target.staticMethod()
    }
    static staticMethod()
    {
        super.staticMethod()
    }
    method1()
    {
        1
    }
    method2(name, {options})
    {
        super.method()
    }
    get getter()
    {
        1
    }
    set setter(x)
    {
        1
    }
    *gen1()
    {
        1
    }
    *gen2()
    {
        1
    }
}
