function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

// Iterator.prototype.flatMap installs an internal wrapper whose `return` forwards
// to the underlying iterator's `return`. Per the spec (and matching IteratorClose
// in ECMA-262), the underlying `return` must be invoked with NO arguments,
// not with `undefined`. See tc39/proposal-explicit-resource-management#273
// for the parallel async-iterator fix.

// Triggered via abrupt completion (`break` inside the for-of) of the helper.
{
    let argsLength = -1;
    let arg0 = "sentinel";
    const baseIterator = {
        count: 0,
        next() {
            if (this.count < 3)
                return { done: false, value: ++this.count };
            return { done: true };
        },
        return(...args) {
            argsLength = args.length;
            arg0 = args[0];
            return { done: true };
        },
    };
    const iter = { [Symbol.iterator]() { return baseIterator; } };

    let pulled = 0;
    for (const item of Iterator.from(iter).flatMap(x => [x])) {
        pulled++;
        if (pulled === 1)
            break;
    }
    shouldBe(pulled, 1);
    shouldBe(argsLength, 0);
    shouldBe(arg0, undefined);
}

// Triggered via explicit `.return()` on the helper.
{
    let argsLength = -1;
    const baseIterator = {
        count: 0,
        next() {
            if (this.count < 5)
                return { done: false, value: ++this.count };
            return { done: true };
        },
        return(...args) {
            argsLength = args.length;
            return { done: true };
        },
    };
    const iter = { [Symbol.iterator]() { return baseIterator; } };

    const helper = Iterator.from(iter).flatMap(x => [x]);
    helper.next();
    helper.return();
    shouldBe(argsLength, 0);
}

// Observable via `arguments` (no rest binding).
{
    let argsLength = -1;
    const baseIterator = {
        count: 0,
        next() {
            if (this.count < 3)
                return { done: false, value: ++this.count };
            return { done: true };
        },
        return() {
            argsLength = arguments.length;
            return { done: true };
        },
    };
    const iter = { [Symbol.iterator]() { return baseIterator; } };

    for (const item of Iterator.from(iter).flatMap(x => [x])) {
        break;
    }
    shouldBe(argsLength, 0);
}

// Sanity: when no abrupt completion occurs, `return` is not invoked at all.
{
    let invoked = 0;
    const baseIterator = {
        count: 0,
        next() {
            if (this.count < 3)
                return { done: false, value: ++this.count };
            return { done: true };
        },
        return() {
            invoked++;
            return { done: true };
        },
    };
    const iter = { [Symbol.iterator]() { return baseIterator; } };

    let pulled = 0;
    for (const item of Iterator.from(iter).flatMap(x => [x]))
        pulled++;
    shouldBe(pulled, 3);
    shouldBe(invoked, 0);
}

// null return should be treated the same as undefined: no call, no throw.
{
    const iterated = Object.create(Iterator.prototype);
    let count = 0;
    iterated.next = function() {
        if (count++ < 3) return { done: false, value: count };
        return { done: true };
    };
    iterated.return = null;

    let threw = false;
    try {
        for (const item of iterated.flatMap(x => [x]))
            break;
    } catch (e) {
        threw = true;
    }
    shouldBe(threw, false);
}
