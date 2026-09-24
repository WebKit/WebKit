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
function destructureDefault(array, at, hook) { let [a, b = (hook(), "d")] = array; return a + "," + b; }
function destructureDefault3(array, at, hook) { let [a, b = (hook(), "d"), c] = array; return a + "," + b + "," + c; }
function destructureThrow(array, at, hook) { try { let [a, b = (hook(), (() => { throw new Error("dflt"); })())] = array; } catch (e) { return e.message; } return "no throw"; }
function destructureNested(array, at, hook) { let [[a, b = (hook(), "d")], c] = array; return a + "," + b + "," + c; }
function destructureAssign(array, at, hook) { let o = {}; [o.a, o[(hook(), "b")]] = array; return o.a + "," + o.b; }
function destructureSetter(array, at, hook) { let o = { set a(v) { hook(); } }; let b; [o.a, b] = array; return "" + b; }
`;

const noHook = () => { };
const closedAt = (value) => 'return [object Array Iterator] true {"value":' + value + ',"done":false}';

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

// Every way out of a pattern, with "return" showing up on %ArrayIteratorPrototype%.
run("destructureDefault", "[1, , 3, 4]", 0, "ArrayIteratorPrototype", "1,d", closedAt(3));
run("destructureDefault3", "[1, , 3, 4]", 0, "ArrayIteratorPrototype", "1,d,3", closedAt(4));
run("destructureThrow", "[1, , 3, 4]", 0, "ArrayIteratorPrototype", "dflt", closedAt(3));
run("destructureNested", "[[1, , 3], 2, 3]", 0, "ArrayIteratorPrototype", "1,d,2", closedAt(3) + "|" + closedAt(3));
run("destructureAssign", "[1, 2, 3, 4]", 0, "ArrayIteratorPrototype", "1,2", closedAt(3));
run("destructureSetter", "[1, 2, 3, 4]", 0, "ArrayIteratorPrototype", "2", closedAt(3));

// "return" showing up on Object.prototype instead.
run("destructureDefault", "[1, , 3, 4]", 0, "ObjectPrototype", "1,d", closedAt(3));
