// Once IteratorClose has been made observable in a realm, that realm never again opens a loop over a String without an
// iterator object. So each case here gets a realm of its own: the loop (or pattern) is opened while nothing is observable,
// "return" shows up in the middle, and the close that follows has to call it on a real String Iterator at the right position.

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
function breakOut(string, at, hook) { let seen = []; for (let x of string) { seen.push(x); if (x === at) { hook(); break; } } return seen.join(); }
function returnOut(string, at, hook) { let seen = []; for (let x of string) { seen.push(x); if (x === at) { hook(); return seen.join(); } } return seen.join(); }
function throwOut(string, at, hook) { let seen = []; try { for (let x of string) { seen.push(x); if (x === at) { hook(); throw new Error("out"); } } } catch (e) { seen.push(e.message); } return seen.join(); }
function continueOuter(string, at, hook) { let seen = []; outer: for (let i = 0; i < 2; i++) { for (let x of string) { seen.push(x); if (x === at) { hook(); continue outer; } } } return seen.join(); }
function finallyOut(string, at, hook) { let seen = []; try { for (let x of string) { seen.push(x); if (x === at) { hook(); return seen.join(); } } } finally { seen.push("finally"); log.push("finally " + seen.join()); } return seen.join(); }
function runToEnd(string, at, hook) { let seen = []; for (let x of string) { seen.push(x); if (x === at) hook(); } return seen.join(); }
function nested(string, at, hook) { let seen = []; for (let x of string) { for (let y of string) { seen.push(x + y); if (y === at) { hook(); break; } } if (x === at) break; } return seen.join(); }
function* each(string, at, hook) { for (let x of string) { if (x === at) hook(); yield x; } }
function generatorReturn(string, at, hook) { let seen = []; let generator = each(string, at, hook); seen.push(generator.next().value); seen.push(generator.next().value); generator.return(); return seen.join(); }
`;

const noHook = () => { };
const closedAt = (value) => 'return [object String Iterator] true {"value":' + JSON.stringify(value) + ',"done":false}';
const closedWhenDone = 'return [object String Iterator] true {"done":true}';

function run(name, string, at, holderName, expected, expectedLog) {
    let realm = createGlobalObject();
    realm.eval(source);
    let f = realm[name];
    for (let i = 0; i < testLoopCount; i++)
        f(string, at, noHook);
    shouldBe(realm.log.filter(s => s.startsWith("return")).length, 0, name + " quiet");
    realm.log.length = 0;

    let message = name + " with return on " + holderName;
    shouldBe(f(string, at, () => realm.installReturn(realm.holders[holderName])), expected, message + " result");
    shouldBe(realm.log.join("|"), expectedLog, message + " log");
    // Each close saw its own object.
    shouldBe(new Set(realm.seenIterators).size, realm.seenIterators.length, message + " distinct iterators");
}

// Every way out of a loop, with "return" showing up on %StringIteratorPrototype%.
run("breakOut", "abcd", "b", "StringIteratorPrototype", "a,b", closedAt("c"));
run("returnOut", "abcd", "c", "StringIteratorPrototype", "a,b,c", closedAt("d"));
run("throwOut", "abcd", "a", "StringIteratorPrototype", "a,out", closedAt("b"));
run("continueOuter", "abcd", "d", "StringIteratorPrototype", "a,b,c,d,a,b,c,d", closedWhenDone + "|" + closedWhenDone);
run("finallyOut", "abcd", "b", "StringIteratorPrototype", "a,b", closedAt("c") + "|finally a,b,finally");
run("runToEnd", "abcd", "b", "StringIteratorPrototype", "a,b,c,d", "");
run("nested", "abcd", "b", "StringIteratorPrototype", "aa,ab,ba,bb", closedAt("c") + "|" + closedAt("c") + "|" + closedAt("c"));
run("generatorReturn", "abcd", "b", "StringIteratorPrototype", "a,b", closedAt("c"));

// The position counts code units, and the iterator created for the close has to continue after a whole code point.
run("breakOut", "a\u{1F600}b\u{1F601}", "\u{1F600}", "StringIteratorPrototype", "a,\u{1F600}", closedAt("b"));
run("breakOut", "a\u{1F600}\u{1F601}", "\u{1F600}", "StringIteratorPrototype", "a,\u{1F600}", closedAt("\u{1F601}"));
run("breakOut", "a\ud83db", "\ud83d", "StringIteratorPrototype", "a,\ud83d", closedAt("b"));
run("breakOut", "ab", "b", "StringIteratorPrototype", "a,b", closedWhenDone);

// "return" showing up on %IteratorPrototype% or Object.prototype instead.
run("breakOut", "abcd", "b", "IteratorPrototype", "a,b", closedAt("c"));
run("breakOut", "abcd", "b", "ObjectPrototype", "a,b", closedAt("c"));
