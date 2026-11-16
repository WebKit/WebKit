// Coverage for %RegExpStringIteratorPrototype%.next now that it is implemented in C++.
// Tests in this file should throw on failure; passing tests must produce no output.

function shouldBe(actual, expected) {
    if (JSON.stringify(actual) !== JSON.stringify(expected))
        throw new Error("Expected " + JSON.stringify(expected) + " but got " + JSON.stringify(actual));
}

function shouldThrow(fn, ctor) {
    let threw = false;
    try { fn(); } catch (e) {
        threw = true;
        if (ctor && !(e instanceof ctor))
            throw new Error("Expected " + ctor.name + ", got " + e);
    }
    if (!threw)
        throw new Error("Expected throw");
}

const RegExpStringIteratorPrototype = Object.getPrototypeOf("".matchAll(/x/g));
const next = RegExpStringIteratorPrototype.next;

// Function identity, name, and length.
shouldBe(typeof next, "function");
shouldBe(next.length, 0);
shouldBe(next.name, "next");
shouldBe(Object.getOwnPropertyDescriptor(RegExpStringIteratorPrototype, "next").enumerable, false);

// Steps 1-3: brand check on the receiver.
shouldThrow(() => next.call(undefined), TypeError);
shouldThrow(() => next.call(null), TypeError);
shouldThrow(() => next.call(42), TypeError);
shouldThrow(() => next.call("string"), TypeError);
shouldThrow(() => next.call({}), TypeError);
shouldThrow(() => next.call(RegExpStringIteratorPrototype), TypeError);
shouldThrow(() => next.call([][Symbol.iterator]()), TypeError);

// Basic global iteration through String.prototype.matchAll.
{
    const matches = [...("a1b2c3".matchAll(/[a-z](\d)/g))];
    shouldBe(matches.length, 3);
    shouldBe(matches[0][0], "a1");
    shouldBe(matches[0][1], "1");
    shouldBe(matches[0].index, 0);
    shouldBe(matches[2][0], "c3");
    shouldBe(matches[2].index, 4);
}

// Step 4: once exhausted, the iterator stays done.
{
    const iterator = "ab".matchAll(/[ab]/g);
    shouldBe(iterator.next().done, false);
    shouldBe(iterator.next().done, false);
    shouldBe(iterator.next(), { value: undefined, done: true });
    shouldBe(iterator.next(), { value: undefined, done: true });
}

// Step 11.b: non-global matcher (via direct @@matchAll call) is done after the first match.
{
    const iterator = /\d/[Symbol.matchAll]("123");
    const first = iterator.next();
    shouldBe(first.done, false);
    shouldBe(first.value[0], "1");
    shouldBe(iterator.next(), { value: undefined, done: true });
    shouldBe(iterator.next(), { value: undefined, done: true });
}

// Step 10: no match at all yields a single done result.
{
    const iterator = "abc".matchAll(/\d/g);
    shouldBe(iterator.next(), { value: undefined, done: true });
}

// Step 11.a.ii: empty matches advance lastIndex by one code unit without 'u'.
{
    const matches = [...("ab".matchAll(/(?:)/g))];
    shouldBe(matches.length, 3);
    shouldBe(matches.map((m) => m.index), [0, 1, 2]);
}

// Step 11.a.ii: empty matches advance by code point with 'u' and 'v' (surrogate pair stays together).
{
    shouldBe([...("a\u{1F600}b".matchAll(/(?:)/g))].length, 5);
    shouldBe([...("a\u{1F600}b".matchAll(/(?:)/gu))].length, 4);
    shouldBe([...("a\u{1F600}b".matchAll(/(?:)/gv))].length, 4);
}

// Step 9: a custom exec on the matcher is honored (slow path), called once per next().
{
    let execCalls = 0;
    class TraceExec extends RegExp {
        exec(str) {
            execCalls++;
            return RegExp.prototype.exec.call(this, str);
        }
    }
    const matches = [...("a1b2".matchAll(new TraceExec("[a-z]", "g")))];
    shouldBe(matches.length, 2);
    // Two successful matches plus the final null-returning call.
    shouldBe(execCalls, 3);
}

// Custom exec returning a non-object, non-null value throws TypeError.
{
    class BadExec extends RegExp {
        exec() { return 42; }
    }
    const iterator = "abc".matchAll(new BadExec("a", "g"));
    shouldThrow(() => iterator.next(), TypeError);
}

// Custom exec returning null finishes the iteration.
{
    class NullExec extends RegExp {
        exec() { return null; }
    }
    const iterator = "abc".matchAll(new NullExec("a", "g"));
    shouldBe(iterator.next(), { value: undefined, done: true });
}

// Step 11.a.i: Get(match, "0") and ToString are observable when exec returns a custom object.
{
    let toStringCalls = 0;
    let zeroGets = 0;
    class FakeExec extends RegExp {
        exec() {
            this.lastIndex = 1;
            return new Proxy({
                get 0() { return { toString() { toStringCalls++; return "x"; } }; }
            }, {
                get(target, key, receiver) {
                    if (key === "0")
                        zeroGets++;
                    return Reflect.get(target, key, receiver);
                }
            });
        }
    }
    const iterator = "abc".matchAll(new FakeExec("a", "g"));
    const result = iterator.next();
    shouldBe(result.done, false);
    if (zeroGets < 1)
        throw new Error("match[0] must be read");
    if (toStringCalls < 1)
        throw new Error("ToString(match[0]) must be performed");
}

// Step 11.a.ii.1: ToLength(Get(R, "lastIndex")) on the iterated matcher is observable when the
// match is empty. A custom species lets us keep a handle on the matcher held by the iterator.
{
    let valueOfCalls = 0;
    let matcher;
    class EmptySpecies extends RegExp {
        static get [Symbol.species]() {
            return function (pattern, flags) {
                matcher = new RegExp(pattern, flags);
                matcher.exec = function () { return [""]; };
                return matcher;
            };
        }
    }
    const iterator = "abc".matchAll(new EmptySpecies("a", "g"));
    matcher.lastIndex = { valueOf() { valueOfCalls++; return 0; } };
    const result = iterator.next();
    shouldBe(result.done, false);
    shouldBe(valueOfCalls, 1);
    shouldBe(matcher.lastIndex, 1);
}

// Step 11.a.ii.3: Set(R, "lastIndex", nextIndex, true) throws on a non-writable lastIndex.
{
    let matcher;
    class EmptySpecies extends RegExp {
        static get [Symbol.species]() {
            return function (pattern, flags) {
                matcher = new RegExp(pattern, flags);
                matcher.exec = function () { return [""]; };
                return matcher;
            };
        }
    }
    const iterator = "abc".matchAll(new EmptySpecies("a", "g"));
    Object.defineProperty(matcher, "lastIndex", { value: 0, writable: false });
    shouldThrow(() => iterator.next(), TypeError);
}

// The iterator operates on a separate matcher whose lastIndex starts at the original's lastIndex
// (RegExp.prototype[Symbol.matchAll] steps 4-5); iteration never mutates the original regexp.
{
    const original = /[a-z]/g;
    original.lastIndex = 1;
    const matches = [...("ab".matchAll(original))];
    shouldBe(matches.length, 1);
    shouldBe(matches[0][0], "b");
    shouldBe(original.lastIndex, 1);
}

// Iterating with named groups and indices flag.
{
    const matches = [...("x=1, y=2".matchAll(/(?<name>[a-z])=(?<value>\d)/dg))];
    shouldBe(matches.length, 2);
    shouldBe(matches[0].groups.name, "x");
    shouldBe(matches[1].groups.value, "2");
    shouldBe(matches[0].indices[0], [0, 3]);
}

// RegExp.lastMatch and friends reflect the latest successful exec performed by the iterator.
{
    [...("foo bar".matchAll(/[a-z]+/g))];
    shouldBe(RegExp.lastMatch, "bar");
}

// A Proxy matcher (installed via a custom @@species through @@matchAll's slow path) goes through
// the generic RegExpExec path: the 'exec' property is looked up on the proxy each iteration.
{
    const accesses = [];
    class ProxySpecies extends RegExp {
        static get [Symbol.species]() {
            return function (pattern, flags) {
                return new Proxy(new RegExp(pattern, flags), {
                    get(target, key, receiver) {
                        accesses.push(key);
                        const value = Reflect.get(target, key, receiver);
                        return typeof value === "function" ? value.bind(target) : value;
                    },
                    set(target, key, value) {
                        return Reflect.set(target, key, value);
                    }
                });
            };
        }
    }
    const matches = [...("a1b2".matchAll(new ProxySpecies("[a-z]", "g")))];
    shouldBe(matches.length, 2);
    if (!accesses.includes("exec"))
        throw new Error("expected exec to be looked up on the proxy matcher");
}

// Iteration result objects are ordinary objects with value/done own properties.
{
    const iterator = "a".matchAll(/a/g);
    const result = iterator.next();
    shouldBe(Object.getPrototypeOf(result), Object.prototype);
    shouldBe(Object.keys(result), ["value", "done"]);
}
