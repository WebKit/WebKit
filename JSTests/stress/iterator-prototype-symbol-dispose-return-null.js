//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

// %IteratorPrototype%[@@dispose] uses GetMethod(O, "return"), which treats
// null the same as undefined (ECMA-262 7.3.10 step 3). When `return` is null,
// the method should no-op rather than attempting to call null.

const IteratorPrototype = Object.getPrototypeOf(Object.getPrototypeOf([][Symbol.iterator]()));

// return: null → no-op, no throw
{
    const iter = Object.create(IteratorPrototype);
    iter.return = null;
    iter[Symbol.dispose]();
}

// return: undefined → no-op (existing behavior)
{
    const iter = Object.create(IteratorPrototype);
    iter.return = undefined;
    iter[Symbol.dispose]();
}

// return: callable → still invoked (no regression)
{
    let called = false;
    const iter = Object.create(IteratorPrototype);
    iter.return = function() { called = true; return {}; };
    iter[Symbol.dispose]();
    shouldBe(called, true);
}

// return: non-callable non-null → still TypeError (not GetMethod's job, Call throws)
{
    const iter = Object.create(IteratorPrototype);
    iter.return = 42;
    let threw = false;
    try { iter[Symbol.dispose](); } catch (e) { threw = e instanceof TypeError; }
    shouldBe(threw, true);
}

// `using` declaration with an iterator whose return is null → dispose succeeds
{
    function* gen() { yield 1; yield 2; }
    const iter = gen();
    iter.return = null;
    {
        using x = iter;
    }
}
