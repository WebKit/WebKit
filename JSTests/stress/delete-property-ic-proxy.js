//@ requireOptions("--jitPolicyScale=0", "--useDFGJIT=0")

{
    var obj1 = this
    function foo1() {
        for (let i = 0; i < 5; ++i)
            delete obj1.x
    }
    noInline(foo1)

    foo1()
    Object.defineProperty(obj1, "x", {})
    foo1()
}

{
    var obj2 = new Proxy({}, {})
    function foo2() {
        for (let i = 0; i < 5; ++i)
            delete obj2.x
    }
    noInline(foo2)

    foo2()
    Object.defineProperty(obj2, "x", {})
    foo2()
}

{
    var obj3 = $vm.createProxy({})
    function foo3() {
        for (let i = 0; i < 5; ++i)
            delete obj3.x
    }
    noInline(foo3)

    foo3()
    Object.defineProperty(obj3, "x", {})
    foo3()
}