// The methods of the iteration protocol get replaced in the middle of a loop over a String that runs without an iterator
// object. Each case gets a realm of its own, since the realm stops opening loops that way afterwards.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const source = `
var log = [];
const StringIteratorPrototype = Object.getPrototypeOf(""[Symbol.iterator]());
const originalNext = StringIteratorPrototype.next;
function installReturn() {
    StringIteratorPrototype.return = function () {
        log.push("return " + Object.prototype.toString.call(this) + " " + JSON.stringify(originalNext.call(this)));
        return {};
    };
}
var nextCalls = 0;
function nextReplacedMidLoop(string, replace) {
    let seen = [];
    for (let x of string) {
        seen.push(x);
        if (replace && x === "b")
            StringIteratorPrototype.next = function () { nextCalls++; return originalNext.call(this); };
    }
    return seen.join();
}
function symbolIteratorReplacedMidLoop(string, replace) {
    let seen = [];
    for (let x of string) {
        seen.push(x);
        if (replace && x === "b")
            String.prototype[Symbol.iterator] = function* () { yield "replaced"; };
    }
    return seen.join();
}
function breakAtTwo(iterable, hook) { let seen = []; for (let x of iterable) { seen.push(x); if (seen.length === 2) { hook(); break; } } return seen.join(); }
`;

const noHook = () => { };

// The spec fixes the next method when the loop is opened: replacing it in the middle must not change what the loop sees.
{
    let realm = createGlobalObject();
    realm.eval(source);
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(realm.nextReplacedMidLoop("abcd", false), "a,b,c,d");
    shouldBe(realm.nextReplacedMidLoop("abcd", true), "a,b,c,d");
    shouldBe(realm.nextCalls, 0);
    shouldBe(realm.nextReplacedMidLoop("abcd", false), "a,b,c,d");
    shouldBe(realm.nextCalls, 5);
}
{
    let realm = createGlobalObject();
    realm.eval(source);
    for (let i = 0; i < testLoopCount; i++)
        shouldBe(realm.symbolIteratorReplacedMidLoop("abcd", false), "a,b,c,d");
    shouldBe(realm.symbolIteratorReplacedMidLoop("abcd", true), "a,b,c,d");
    shouldBe(realm.symbolIteratorReplacedMidLoop("abcd", false), "replaced");
}

// Making IteratorClose observable for Strings leaves loops over Arrays alone.
{
    let realm = createGlobalObject();
    realm.eval(source);
    for (let i = 0; i < testLoopCount; i++) {
        shouldBe(realm.breakAtTwo("abc", noHook), "a,b");
        shouldBe(realm.breakAtTwo([1, 2, 3], noHook), "1,2");
    }
    shouldBe(realm.breakAtTwo("abc", realm.installReturn), "a,b");
    shouldBe(realm.log.join("|"), 'return [object String Iterator] {"value":"c","done":false}');
    for (let i = 0; i < testLoopCount; i++) {
        shouldBe(realm.breakAtTwo([1, 2, 3], noHook), "1,2");
        shouldBe(realm.breakAtTwo("abc", noHook), "a,b");
    }
    shouldBe(realm.log.length, 1 + testLoopCount);
}
