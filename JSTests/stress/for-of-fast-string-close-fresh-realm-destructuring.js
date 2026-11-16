// Once IteratorClose has been made observable in a realm, that realm never again opens a pattern over a String without an
// iterator object. So each case here gets a realm of its own: "return" shows up after the function has run many times without
// it, and the close that follows has to call it on a real String Iterator at the right position.

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
function firstTwo(string, hook) { let [a, b = hook()] = string; return a + "," + b; }
function firstOnly(string, hook) { let [a = hook()] = string; return String(a); }
function none(string, hook) { hook(); let [] = string; return ""; }
`;

const noHook = () => { };
const closedAt = (value) => 'return [object String Iterator] {"value":' + JSON.stringify(value) + ',"done":false}';

function run(name, string, installBeforeCall, expected, expectedLog) {
    let realm = createGlobalObject();
    realm.eval(source);
    let f = realm[name];
    for (let i = 0; i < testLoopCount; i++)
        f(string, noHook);
    shouldBe(realm.log.length, 0, name + " quiet");

    if (installBeforeCall)
        realm.installReturn();
    shouldBe(f(string, realm.installReturn), expected, name + " result");
    shouldBe(realm.log.join("|"), expectedLog, name + " log");
}

// The default is evaluated once the String is exhausted, and then there is nothing to close.
run("firstTwo", "a", false, "a,undefined", "");
run("firstOnly", "", false, "undefined", "");

// The default is not evaluated, but the pattern is not exhausted either, so IteratorClose runs.
run("firstTwo", "abc", false, "a,b", "");
run("firstTwo", "abc", true, "a,b", closedAt("c"));
run("firstTwo", "a\u{1F600}b", true, "a,\u{1F600}", closedAt("b"));
run("firstOnly", "abc", true, "a", closedAt("b"));
run("none", "abc", false, "", closedAt("a"));
