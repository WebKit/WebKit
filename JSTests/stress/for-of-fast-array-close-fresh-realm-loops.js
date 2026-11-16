// Once IteratorClose has been made observable in a realm, that realm never again opens a loop over an Array without an
// iterator object. So each case here gets a realm of its own: the loop (or pattern) is opened while nothing is observable,
// "return" shows up in the middle, and the close that follows has to call it on a real Array Iterator at the right position.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const source = `
var log = [];
var seenIterators = [];
const ArrayIteratorPrototype = Object.getPrototypeOf([][Symbol.iterator]());
const IteratorPrototype = Object.getPrototypeOf(ArrayIteratorPrototype);
const originalNext = ArrayIteratorPrototype.next;
var holders = { ArrayIteratorPrototype, IteratorPrototype, ObjectPrototype: Object.prototype };
function installReturn(where) {
    where.return = function () {
        let step = originalNext.call(this);
        log.push("return " + Object.prototype.toString.call(this) + " " + (Object.getPrototypeOf(this) === ArrayIteratorPrototype) + " " + JSON.stringify(step));
        seenIterators.push(this);
        return {};
    };
}
function breakOut(array, at, hook) { let seen = []; for (let x of array) { seen.push(x); if (x === at) { hook(); break; } } return seen.join(); }
function returnOut(array, at, hook) { let seen = []; for (let x of array) { seen.push(x); if (x === at) { hook(); return seen.join(); } } return seen.join(); }
function throwOut(array, at, hook) { let seen = []; try { for (let x of array) { seen.push(x); if (x === at) { hook(); throw new Error("out"); } } } catch (e) { seen.push(e.message); } return seen.join(); }
function continueOuter(array, at, hook) { let seen = []; outer: for (let i = 0; i < 2; i++) { for (let x of array) { seen.push(x); if (x === at) { hook(); continue outer; } } } return seen.join(); }
function breakOuter(array, at, hook) { let seen = []; outer: for (let i = 0; i < 2; i++) { for (let x of array) { seen.push(x); if (x === at) { hook(); break outer; } } } return seen.join(); }
function finallyOut(array, at, hook) { let seen = []; try { for (let x of array) { seen.push(x); if (x === at) { hook(); return seen.join(); } } } finally { seen.push("finally"); log.push("finally " + seen.join()); } return seen.join(); }
function runToEnd(array, at, hook) { let seen = []; for (let x of array) { seen.push(x); if (x === at) hook(); } return seen.join(); }
function nested(array, at, hook) { let seen = []; for (let x of array) { for (let y of array) { seen.push(x * 10 + y); if (y === at) { hook(); break; } } if (x === at) break; } return seen.join(); }

var nextCalls = 0;
function nextReplacedMidLoop(array, replace) {
    let seen = [];
    for (let x of array) {
        seen.push(x);
        if (replace && x === 2)
            ArrayIteratorPrototype.next = function () { nextCalls++; return originalNext.call(this); };
    }
    return seen.join();
}
`;

const noHook = () => { };
const closedAt = (value) => 'return [object Array Iterator] true {"value":' + value + ',"done":false}';
const closedWhenDone = 'return [object Array Iterator] true {"done":true}';

function run(name, arraySource, at, holderName, expected, expectedLog) {
    let realm = createGlobalObject();
    realm.eval(source);
    let f = realm[name];
    let make = realm.eval("(function () { return " + arraySource + "; })");
    for (let i = 0; i < testLoopCount; i++)
        f(make(), at, noHook);
    shouldBe(realm.log.filter(s => s.startsWith("return")).length, 0, name + " quiet");
    realm.log.length = 0;

    let message = name + " with return on " + holderName;
    shouldBe(f(make(), at, () => realm.installReturn(realm.holders[holderName])), expected, message + " result");
    shouldBe(realm.log.join("|"), expectedLog, message + " log");
    // Each close saw its own object.
    shouldBe(new Set(realm.seenIterators).size, realm.seenIterators.length, message + " distinct iterators");
}

// Every way out of a loop, with "return" showing up on %ArrayIteratorPrototype%.
run("breakOut", "[1, 2, 3, 4]", 2, "ArrayIteratorPrototype", "1,2", closedAt(3));
run("returnOut", "[1, 2, 3, 4]", 3, "ArrayIteratorPrototype", "1,2,3", closedAt(4));
run("throwOut", "[1, 2, 3, 4]", 1, "ArrayIteratorPrototype", "1,out", closedAt(2));
run("continueOuter", "[1, 2, 3, 4]", 4, "ArrayIteratorPrototype", "1,2,3,4,1,2,3,4", closedWhenDone + "|" + closedWhenDone);
run("breakOuter", "[1, 2, 3, 4]", 2, "ArrayIteratorPrototype", "1,2", closedAt(3));
run("finallyOut", "[1, 2, 3, 4]", 2, "ArrayIteratorPrototype", "1,2", closedAt(3) + "|finally 1,2,finally");
run("runToEnd", "[1, 2, 3, 4]", 2, "ArrayIteratorPrototype", "1,2,3,4", "");
run("nested", "[1, 2, 3, 4]", 2, "ArrayIteratorPrototype", "11,12,21,22", closedAt(3) + "|" + closedAt(3) + "|" + closedAt(3));

// "return" showing up on %IteratorPrototype% instead.
run("breakOut", "[1, 2, 3, 4]", 2, "IteratorPrototype", "1,2", closedAt(3));

// The spec fixes the next method when the loop is opened: replacing it in the middle of a loop that runs without an iterator
// object changes nothing for that loop. Loops opened afterwards call the replacement.
{
    let realm = createGlobalObject();
    realm.eval(source);
    let make = realm.eval("(function () { return [1, 2, 3, 4]; })");
    for (let i = 0; i < testLoopCount; i++)
        realm.nextReplacedMidLoop(make(), false);
    shouldBe(realm.nextReplacedMidLoop(make(), true), "1,2,3,4");
    shouldBe(realm.nextCalls, 0);
    shouldBe(realm.nextReplacedMidLoop(make(), true), "1,2,3,4");
    shouldBe(realm.nextCalls, 5);
}
