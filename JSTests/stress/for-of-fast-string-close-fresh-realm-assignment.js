// The elements of a String are never undefined, so a default value cannot make IteratorClose observable while a pattern is
// in the middle of a String. An assignment target can: "return" shows up after the pattern was opened without an iterator
// object, and the close that follows has to call it on a real String Iterator at the right position.
// Each case gets a realm of its own, since a realm where IteratorClose is observable never opens a pattern that way again.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const source = `
var log = [];
var seenIterators = [];
const StringIteratorPrototype = Object.getPrototypeOf(""[Symbol.iterator]());
const IteratorPrototype = Object.getPrototypeOf(StringIteratorPrototype);
const originalNext = StringIteratorPrototype.next;
var holders = { StringIteratorPrototype, IteratorPrototype, ObjectPrototype: Object.prototype };
function installReturn(where) {
    where.return = function () {
        let step = originalNext.call(this);
        log.push("return " + Object.prototype.toString.call(this) + " " + (Object.getPrototypeOf(this) === StringIteratorPrototype) + " " + JSON.stringify(step));
        seenIterators.push(this);
        return {};
    };
}
function destructureAssign(string, hook) { let o = {}; [o.a, o[(hook(), "b")]] = string; return o.a + "," + o.b; }
function destructureSetter(string, hook) { let o = { set a(v) { hook(); } }; let b; [o.a, b] = string; return "" + b; }
function destructureThrow(string, hook) { let o = { set a(v) { hook(); throw new Error("setter"); } }; try { [o.a] = string; } catch (e) { return e.message; } return "no throw"; }
function destructureNested(string, hook) { let o = { set a(v) { hook(); } }; let b; [[o.a], b] = string; return "" + b; }
`;

const noHook = () => { };
const closedAt = (value) => 'return [object String Iterator] true {"value":' + JSON.stringify(value) + ',"done":false}';
const closedWhenDone = 'return [object String Iterator] true {"done":true}';

function run(name, string, holderName, expected, expectedLog) {
    let realm = createGlobalObject();
    realm.eval(source);
    let f = realm[name];
    for (let i = 0; i < testLoopCount; i++)
        f(string, noHook);
    shouldBe(realm.log.length, 0, name + " quiet");

    let message = name + " with return on " + holderName;
    shouldBe(f(string, () => realm.installReturn(realm.holders[holderName])), expected, message + " result");
    shouldBe(realm.log.join("|"), expectedLog, message + " log");
    // Each close saw its own object.
    shouldBe(new Set(realm.seenIterators).size, realm.seenIterators.length, message + " distinct iterators");
}

run("destructureAssign", "abcd", "StringIteratorPrototype", "a,b", closedAt("c"));
run("destructureSetter", "abcd", "StringIteratorPrototype", "b", closedAt("c"));
run("destructureSetter", "ab", "StringIteratorPrototype", "b", closedWhenDone);
run("destructureThrow", "abcd", "StringIteratorPrototype", "setter", closedAt("b"));
run("destructureNested", "abcd", "StringIteratorPrototype", "b", closedWhenDone + "|" + closedAt("c"));

// The position counts code units, and the iterator created for the close has to continue after a whole code point.
run("destructureSetter", "a\u{1F600}b", "StringIteratorPrototype", "\u{1F600}", closedAt("b"));

// "return" showing up on Object.prototype instead.
run("destructureSetter", "abcd", "ObjectPrototype", "b", closedAt("c"));
