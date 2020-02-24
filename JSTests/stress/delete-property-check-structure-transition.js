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

function sd(obj) {
    let data = $vm.getStructureTransitionList(obj)
    let result = []

    if (!data)
        return result

    for (let i = 0; i < data.length/5; ++i) {
        result.push({ id: data[i*5+0], offset: data[i*5+1], max: data[i*5+2], property: data[i*5+3], type: data[i*5+4] == 0 ? "added" : "deleted" })
    }

    return result
}

function sid(obj) {
    let data = sd(obj)
    return data[data.length-1].id
}

function testDeleteIsNotUncacheable(i) {
    let foo = {}
    foo["bar" + i] = 1
    foo["baz" + i] = 2
    assert(foo["bar" + i] === 1)
    assert(foo["baz" + i] === 2)
    let oldSid = sid(foo)

    assert_eq($vm.getConcurrently(foo, "baz"+i), 1)
    assert_eq($vm.getConcurrently(foo, "bar"+i), 1)
    assert_eq($vm.getConcurrently(foo, "foo"+i), 0)

    assert(delete foo["bar" + i])
    assert_neq(oldSid, sid(foo))
    assert(!(("bar" + i) in foo));
    assert(foo["baz" + i] === 2)

    assert_eq($vm.getConcurrently(foo, "baz"+i), 1)
    assert_eq($vm.getConcurrently(foo, "bar"+i), 0)

    assert_eq(Object.keys(foo).length, 1)

    let data = sd(foo)
    assert_eq(data[data.length-1].property, "bar" + i)

    foo["bar" + i] = 1
    assert_eq($vm.getConcurrently(foo, "baz"+i), 1)
    assert_eq($vm.getConcurrently(foo, "bar"+i), 1)
    assert(foo["bar" + i] === 1)
}

function testCanMaterializeDeletes(i) {
    let foo = {}
    foo["bar" + i] = 1
    foo["baz" + i] = 2

    assert(foo["bar" + i] === 1)
    assert(foo["baz" + i] === 2)
    assert(delete foo["bar" + i])
    assert(!("bar" + i in foo))
    assert(foo["baz" + i] === 2)
    assert_eq(Object.keys(foo).length, 1)

    let foo2 = {}
    foo2["bar" + i] = 3
    foo2["baz" + i] = 4
    assert(delete foo2["bar" + i])

    assert_eq(sid(foo2), sid(foo))
    foo2["fun" + i] = 3
    assert_neq(sid(foo2), sid(foo))

    assert(foo2["fun" + i] === 3)
    assert(foo2["baz" + i] === 4)
    assert(!("bar" + i in foo2))
    assert_eq(Object.keys(foo2).length, 2)
    assert(foo["baz" + i] === 2)
    assert(!("bar" + i in foo))
    assert_eq(Object.keys(foo).length, 1)

    let data = sd(foo)
    assert_eq(data[data.length-1].property, "bar" + i)

    data = sd(foo2)
    assert_eq(data[data.length-1].property, "fun" + i)
    assert_eq(data[data.length-2].property, "bar" + i)
}

function testCanFlatten(i) {
    let foo = {}
    for (let j=0; j<500; ++j) {
        const oldId = sid(foo)

        foo["x" + 1000*j + i] = j
        if (j > 0)
            delete foo["x" + 1000*(j - 1) + i]

        if (j > 100)
            assert_eq(sid(foo), oldId)
    }

    for (let j=0; j<500; ++j) {
        const val = foo["x" + 1000*j + i]
        if (j == 499)
            assert_eq(val, j)
        else
            assert_eq(val, undefined)
    }

    $vm.flattenDictionaryObject(foo)

    for (let j=0; j<500; ++j) {
        const val = foo["x" + 1000*j + i]
        if (j == 499)
            assert_eq(val, j)
        else
            assert_eq(val, undefined)
    }
}

function testDeleteWithInlineCache() {
    Object.prototype.globalProperty = 42

    function makeFoo() {
        let foo = {}
        foo.baz = 1
        assert(foo.globalProperty === 42)

        return foo
    }
    noInline(makeFoo)

    function doTest(xVal) {
        for (let j=0; j<50; ++j) {
            for (let z=0; z<10000; ++z) {
                const foo = arr[j]

                assert(foo.baz === 1)
                assert_eq(Object.keys(foo).length, 1)
                assert_eq(foo.globalProperty, xVal)
            }
        }
    }
    noInline(doTest)

    arr = new Array(50)

    for (let j=0; j<50; ++j) {
        arr[j] = makeFoo()
        if (j > 0)
            assert_eq(sid(arr[j-1]), sid(arr[j]))
    }

    doTest(42)

    Object.prototype.globalProperty = 43

    doTest(43)

    delete Object.prototype.globalProperty

    doTest(undefined)
}

testDeleteWithInlineCache()

for (let i = 0; i < 1000; ++i) {
    testDeleteIsNotUncacheable(i)
    testCanMaterializeDeletes(1000+i)
}

for (let i = 0; i < 100; ++i) {
    testCanFlatten(2000+i)
}

