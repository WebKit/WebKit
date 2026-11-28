//@ $skipModes << :lockdown # This mode changes the indexing modes that this test checks.
//@ requireOptions("--useDollarVM=1")

function shouldBe(actual, expected) {
    if (!Object.is(actual, expected))
        throw new Error("bad value: " + String(actual) + " expected: " + String(expected));
}

// Every case compiles its own monomorphic functions, and all the cases share polymorphic ones.
const sharedSloppy = new Function("object", "key", "return delete object[key];");
const sharedStrict = new Function("object", "key", "'use strict'; return delete object[key];");
noInline(sharedSloppy);
noInline(sharedStrict);

let counter = 0;

function test(source, mode, key, value, others) {
    // Array allocation profiles learn from what happens to the arrays. Start over now and then, to keep the indexing mode.
    // The comment makes the source unique, since functions with the same source share what they learned.
    const makeCreate = () => {
        const create = new Function("return " + source + "; // " + counter++);
        noInline(create);
        shouldBe($vm.indexingMode(create()), mode);
        return create;
    };
    let create = makeCreate();
    const sloppy = new Function("object", "key", "return delete object[key];");
    const strict = new Function("object", "key", "'use strict'; return delete object[key | 0];");
    const inAndGet = new Function("object", "key", "const deleted = delete object[key]; return deleted && !(key in object) && object[key] === undefined;");
    const functions = [sloppy, strict, inAndGet, sharedSloppy, sharedStrict];
    for (const remove of functions)
        noInline(remove);

    for (let i = 0; i < testLoopCount / 2; ++i) {
        if (!(i % 500))
            create = makeCreate();
        const object = create();
        shouldBe(object[key], value);
        shouldBe(functions[i % functions.length](object, key), true);
        shouldBe(object[key], undefined);
        shouldBe(key in object, false);
    }

    for (const remove of functions) {
        const object = create();
        const length = object.length;
        shouldBe(object[key], value);
        shouldBe(remove(object, key), true);
        shouldBe(remove(object, key), true);
        shouldBe(key in object, false);
        shouldBe(Object.keys(object).join(), others);
        shouldBe(object.length, length);
    }
}

test("(function() { const a = []; for (let i = 0; i < 4; ++i) a.push(i); return a; })()", "ArrayWithInt32", 1, 1, "0,2,3");
test("(function() { const a = []; for (let i = 0; i < 4; ++i) a.push(i + 0.5); return a; })()", "ArrayWithDouble", 3, 3.5, "0,1,2");
test("(function() { const a = []; for (let i = 0; i < 4; ++i) a.push('a'); return a; })()", "ArrayWithContiguous", 0, "a", "1,2,3");
test("({ 0: 0, 1: 1, 2: 2 })", "NonArrayWithInt32", 1, 1, "0,2");
test("({ 0: 0.5, 1: 1.5, 2: 2.5 })", "NonArrayWithDouble", 2, 2.5, "0,1");
test("({ 0: 'a', 1: 'b', named: 'c' })", "NonArrayWithContiguous", 0, "a", "1,named");

// The next evaluation of a copy-on-write literal must not see the hole.
test("[1, 2, 3]", "CopyOnWriteArrayWithInt32", 0, 1, "1,2");
test("[1.5, 2.5, 3.5]", "CopyOnWriteArrayWithDouble", 1, 2.5, "0,2");
test("['a', 'b', 'c']", "CopyOnWriteArrayWithContiguous", 2, "c", "0,1");

test("({ named: 1 })", "NonArray", 0, undefined, "named");
test("new Array(4)", "ArrayWithUndecided", 1, undefined, "");
test("[1, 2, 3]", "CopyOnWriteArrayWithInt32", 100000, undefined, "0,1,2");
test("({ 0: 1 })", "NonArrayWithInt32", 100000, undefined, "0");

test("(function() { const a = [0, 1, 2]; $vm.ensureArrayStorage(a); return a; })()", "ArrayWithArrayStorage", 1, 1, "0,2");
