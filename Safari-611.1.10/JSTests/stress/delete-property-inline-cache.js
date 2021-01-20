function assert(condition) {
    if (!condition)
        throw new Error("assertion failed");
}

function assert_eq(a, b) {
    if (a !== b)
        throw new Error("assertion failed: " + a + " === " + b);
}

function assert_neq(a, b) {
    if (a === b)
        throw new Error("assertion failed: " + a + " !== " + b);
}

function assert_throws(fun) {
    let threw = false;
    try {
        fun()
    } catch {
        threw = true
    }
    assert(threw)
}
noInline(assert)
noInline(assert_eq)
noInline(assert_neq)
noInline(assert_throws)

function testCacheableDeleteById() {
    function makeFoo() {
        let foo = {}
        foo.baz = 1
        foo.bar = 2
        delete foo.baz
        return foo
    }
    noInline(makeFoo)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!("baz" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))
    }
}
noInline(testCacheableDeleteById)

function testCacheableDeleteByVal() {
    function makeFoo2() {
        let foo = {}
        foo.baz = 1
        foo.bar = 2
        delete foo["baz"]
        return foo
    }
    noInline(makeFoo2)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo2()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!("baz" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))

    }
}
noInline(testCacheableDeleteByVal)

function testCacheableEmptyDeleteById() {
    function makeFoo3() {
        let foo = {}
        foo.baz = 1
        foo.bar = 2
        assert_eq(delete foo.bar, true)
        delete foo.baz
        return foo
    }
    noInline(makeFoo3)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo3()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert_eq(Object.keys(foo).length, 0)
        assert(!("baz" in foo))
        assert(!("bar" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))
        assert(!("bar" in foo))
    }
}
noInline(testCacheableEmptyDeleteById)

function testCacheableDeleteByIdMiss() {
    function makeFoo4() {
        let foo = {}
        foo.baz = 1
        foo.bar = 2
        assert_eq(delete foo.baz, true)
        assert_eq(delete foo.baz, true)
        assert_eq(delete foo.baz, true)

        Object.defineProperty(foo, 'foobar', {
          value: 42,
          configurable: false
        });
        assert_eq(delete foo.foobar, false)
        assert_eq(delete foo.foobar, false)
        assert_eq(foo.foobar, 42)
        return foo
    }
    noInline(makeFoo4)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo4()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!("baz" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))
    }
}
noInline(testCacheableDeleteByIdMiss)

function testDeleteIndex() {
    function makeFoo5() {
        let foo = {}
        foo.baz = 1
        foo.bar = 2
        foo[0] = 23
        foo[1] = 25
        assert_eq(delete foo.bar, true)
        assert_eq(delete foo[1], true)
        delete foo.baz
        return foo
    }
    noInline(makeFoo5)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo5()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert_eq(Object.keys(foo).length, 1)
        assert(!("baz" in foo))
        assert(!("bar" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))
        assert(!("bar" in foo))
        assert_eq(foo[0], 23)
        assert_eq(foo[1], undefined)
        foo[1] = 1
        assert_eq(foo[1], 1)
    }
}
noInline(testDeleteIndex)

function testPolymorphicDelByVal(i) {
    function makeFoo6() {
        let foo = {}
        foo["baz"+i] = 1
        foo.bar = 2
        delete foo["baz"+i]
        return foo
    }
    noInline(makeFoo6)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo6()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!("baz"+i in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz"+i in foo))

    }
}
noInline(testPolymorphicDelByVal)

function testBigintDeleteByVal() {
    function makeFoo7() {
        let foo = {}
        foo[1234567890123456789012345678901234567890n] = 1
        foo.bar = 2
        delete foo[1234567890123456789012345678901234567890n]
        return foo
    }
    noInline(makeFoo7)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo7()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!(1234567890123456789012345678901234567890n in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))

    }
}
noInline(testBigintDeleteByVal)

function testSymbolDeleteByVal() {
    const symbol = Symbol('foo');

    function makeFoo8() {
        let foo = {}
        foo[symbol] = 1
        assert(!(Symbol('foo') in foo))
        assert(symbol in foo)
        foo.bar = 2
        delete foo[symbol]
        return foo
    }
    noInline(makeFoo8)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo8()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!(symbol in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))

    }
}
noInline(testSymbolDeleteByVal)

function testObjDeleteByVal() {
    const symbol = { i: "foo" };

    function makeFoo9() {
        let foo = {}
        foo[symbol] = 1
        assert(symbol in foo)
        assert(!({ i: "foo" } in foo))
        foo.bar = 2
        delete foo[symbol]
        return foo
    }
    noInline(makeFoo9)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo9()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!(symbol in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))

    }
}
noInline(testObjDeleteByVal)

function testStrict() {
    "use strict";

    function makeFoo10() {
        let foo = {}
        foo.baz = 1
        foo.bar = 2
        assert_eq(delete foo.baz, true)
        assert_eq(delete foo.baz, true)

        Object.defineProperty(foo, 'foobar', {
          value: 42,
          configurable: false
        });
        assert_throws(() => delete foo.foobar)
        assert_eq(foo.foobar, 42)
        return foo
    }
    noInline(makeFoo10)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = makeFoo10()
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert(foo.bar === 2)
        assert_eq(Object.keys(foo).length, 1)
        assert(!("baz" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))
    }
}
noInline(testStrict)

function testOverride() {
    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = function() { }
        assert_eq($vm.hasOwnLengthProperty(arr[j]), true)
        delete arr[j].length
        assert_eq($vm.hasOwnLengthProperty(arr[j]), false)
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert_eq(Object.keys(foo).length, 0)
        assert(!("baz" in foo))
        foo.bag = 1
        assert(foo.bag == 1)
        foo["bug" + j] = 3
        assert(foo["bug" + j] == 3)
        assert(!("baz" in foo))
    }
}
noInline(testOverride)

function testNonObject() {
    function deleteIt(foo) {
        assert(delete foo.baz)
        return foo
    }
    noInline(deleteIt)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = { baz: j }
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert("baz" in foo)
        deleteIt(foo)
        assert(!("baz" in foo))
    }

    assert(deleteIt("baz") == "baz")
    deleteIt(Symbol("hi"))
}
noInline(testNonObject)

function testNonObjectStrict() {
    "use strict";

    function deleteIt(foo) {
        assert(delete foo["baz"])
        return foo
    }
    noInline(deleteIt)

    arr = new Array(50)

    for (let j = 0; j < 50; ++j) {
        arr[j] = { baz: j }
    }

    for (let j = 0; j < 50; ++j) {
        const foo = arr[j]

        assert("baz" in foo)
        deleteIt(foo)
        deleteIt(5)
        assert(!("baz" in foo))
    }

    assert(deleteIt("baz") == "baz")
    deleteIt(Symbol("hi"))

    let foo = {}
    Object.defineProperty(foo, 'baz', {
      value: 42,
      configurable: false
    });
    assert_throws(() => deleteIt(foo))
}
noInline(testNonObjectStrict)

function testExceptionUnwind() {
    "use strict";

    function mutateThem(a,b,c,d,e,f,g,h,i) {
        g.i = 42
    }
    noInline(mutateThem)

    function deleteIt(foo) {
        let caught = false

        try {
            var a = { i: 1 }
            var v = { i: 10 }
            var b = { i: 100 }
            var c = { i: 1000 }
            var d = { i: 10000 }
            var e = { i: 100000 }
            var f = { i: 1000000 }
            var g = { i: 10000000 }
            var h = { i: 100000000 }
            var i = { i: 1000000000 }

            mutateThem(a,b,c,d,e,f,g,h,i)

            delete foo["baz"]

            a = b = c = d = e = f = g = h = i
        } catch {
            assert_eq(a.i, 1)
            assert_eq(i.i, 1000000000)
            assert_eq(g.i, 42)
            caught = true
        }
        return caught
    }
    noInline(deleteIt)

    for (let j = 0; j < 100000; ++j) {
        assert(!deleteIt({ j, baz: 5 }))
    }

    let foo = {}
    Object.defineProperty(foo, 'baz', {
      value: 42,
      configurable: false
    });

    assert(deleteIt(foo))
}
noInline(testExceptionUnwind)

function testTDZ() {
    delete foo.x
    let foo = { x: 5 }
}
noInline(testTDZ)

function testPolyProto() {
    function makeFoo11() {
        class C {
            constructor() {
                this.x = 42
            }
        }
        return new C
    }
    noInline(makeFoo11)

    for (let i = 0; i < 1000; ++i) {
        let o = makeFoo11()
        assert_eq(o.x, 42)
        assert(delete o.x)
        assert(!("x" in o))

    }
}
noInline(testPolyProto)

for (let i = 0; i < 1000; ++i) {
    testCacheableDeleteById()
    testCacheableDeleteByVal()
    testCacheableEmptyDeleteById()
    testCacheableDeleteByIdMiss()
    testDeleteIndex()
    testPolymorphicDelByVal(i)
    testBigintDeleteByVal()
    testSymbolDeleteByVal()
    testStrict()
    testOverride()
    testNonObject()
    testNonObjectStrict()
    assert_throws(testTDZ)
    testPolyProto()
}

testExceptionUnwind()
