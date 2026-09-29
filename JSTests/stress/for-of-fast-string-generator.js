function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

function* each(string) { for (let x of string) yield x; }
function* eachTwice(string) { for (let x of string) { for (let y of string) yield x + y; } }
function* firstTwo(string) { let [a, b] = string; yield a; yield b; }
async function eachAwaited(string) { let seen = []; for (let x of string) seen.push(await x); return seen.join("|"); }

for (let i = 0; i < testLoopCount; i++) {
    shouldBe([...each("a\u{1F600}b")].join("|"), "a|\u{1F600}|b");
    shouldBe([...eachTwice("a\u{1F600}")].join("|"), "aa|a\u{1F600}|\u{1F600}a|\u{1F600}\u{1F600}");
    shouldBe([...firstTwo("\u{1F600}xy")].join("|"), "\u{1F600}|x");

    // Two generators over the same String, each suspended in the middle of its loop.
    let first = each("a\u{1F600}b"), second = each("a\u{1F600}b");
    shouldBe(first.next().value, "a");
    shouldBe(second.next().value, "a");
    shouldBe(first.next().value, "\u{1F600}");
    shouldBe(first.return(1).value, 1);
    shouldBe(first.next().done, true);
    shouldBe(second.next().value, "\u{1F600}");
    shouldBe(second.next().value, "b");
    shouldBe(second.next().done, true);
}

let awaited;
for (let i = 0; i < testLoopCount; i++)
    eachAwaited("a\u{1F600}b").then(value => { awaited = value; });
drainMicrotasks();
shouldBe(awaited, "a|\u{1F600}|b");
