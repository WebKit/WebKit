// op_iterator_next and op_iterator_close_check with wide16 and wide32 operands, over a String that is iterated without an
// iterator object.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function makeWithLocals(count) {
    let names = [];
    for (let i = 0; i < count; i++)
        names.push("v" + i);
    return eval(`(function (string, wanted, hook) {
        let ${names.join(",")};
        let seen = [];
        for (let x of string) {
            seen.push(x);
            if (x === wanted) {
                hook();
                break;
            }
        }
        let [a, b] = string;
        return seen.join("") + "|" + a + b;
    })`);
}

const StringIteratorPrototype = Object.getPrototypeOf(""[Symbol.iterator]());
const originalNext = StringIteratorPrototype.next;
let log = [];
function installReturn() {
    StringIteratorPrototype.return = function () { log.push(JSON.stringify(originalNext.call(this))); return {}; };
}
const noHook = () => { };

let wide16 = makeWithLocals(200);
let wide32 = makeWithLocals(33000);
noInline(wide16);
noInline(wide32);

for (let i = 0; i < testLoopCount; i++) {
    shouldBe(wide16("abcd", "c", noHook), "abc|ab");
    shouldBe(wide16("a\u{1F600}b", "z", noHook), "a\u{1F600}b|a\u{1F600}");
}
for (let i = 0; i < 10; i++) {
    shouldBe(wide32("abcd", "c", noHook), "abc|ab");
    shouldBe(wide32("a\u{1F600}b", "z", noHook), "a\u{1F600}b|a\u{1F600}");
}
shouldBe(log.length, 0);

// IteratorClose becomes observable in the middle of the loop. The pattern that follows is opened with an iterator object.
shouldBe(wide32("abcd", "b", installReturn), "ab|ab");
shouldBe(log.join("|"), '{"value":"c","done":false}|{"value":"c","done":false}');
log = [];
shouldBe(wide16("abcd", "b", noHook), "ab|ab");
shouldBe(log.join("|"), '{"value":"c","done":false}|{"value":"c","done":false}');
