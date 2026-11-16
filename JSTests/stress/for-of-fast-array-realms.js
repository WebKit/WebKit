// The Array Iterator protocol is a per-realm thing. An Array of another realm is iterated with that realm's iterator
// (always an object here), and what the other realm does to its prototypes must not leak into loops over this realm's Arrays.

function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error((message ? message + ": " : "") + "expected " + expected + " but got " + actual);
}

const other = createGlobalObject();
let log = [];
other.log = log;

function firstTwo(array) { let seen = []; for (let x of array) { seen.push(x); if (seen.length === 2) break; } return seen.join(); }
function pair(array) { let [a, b] = array; return a + "," + b; }
noInline(firstTwo); noInline(pair);

let foreign = other.eval("[10, 20, 30]");
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(firstTwo([1, 2, 3]), "1,2");
    shouldBe(firstTwo(foreign), "10,20");
    shouldBe(pair([1, 2, 3]), "1,2");
    shouldBe(pair(foreign), "10,20");
}
shouldBe(log.length, 0);

// The other realm makes its IteratorClose observable and replaces its iterator method.
other.eval(`
    Object.prototype.return = function () { log.push("other return " + Object.prototype.toString.call(this)); return {}; };
`);
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(firstTwo([1, 2, 3]), "1,2");
    shouldBe(pair([1, 2, 3]), "1,2");
}
shouldBe(log.length, 0, "this realm is unaffected");
shouldBe(firstTwo(foreign), "10,20");
shouldBe(pair(foreign), "10,20");
shouldBe(log.join("|"), "other return [object Array Iterator]|other return [object Array Iterator]");
log.length = 0;

other.eval(`
    delete Object.prototype.return;
    Array.prototype[Symbol.iterator] = function () { log.push("other iterator"); let i = 0; let self = this; return { next() { return i < self.length ? { value: self[i++] * 2, done: false } : { value: undefined, done: true }; } }; };
`);
for (let i = 0; i < testLoopCount; i++) {
    shouldBe(firstTwo([1, 2, 3]), "1,2");
    shouldBe(pair([1, 2, 3]), "1,2");
}
shouldBe(log.length, 0, "this realm is still unaffected");
shouldBe(firstTwo(foreign), "20,40");
shouldBe(pair(foreign), "20,40");
shouldBe(log.join("|"), "other iterator|other iterator");
log.length = 0;

// A function of the other realm looping over this realm's Array uses its own realm's protocol state.
let otherFirstTwo = other.eval("(function (array) { let seen = []; for (let x of array) { seen.push(x); if (seen.length === 2) break; } return seen.join(); })");
for (let i = 0; i < testLoopCount; i++)
    shouldBe(otherFirstTwo([1, 2, 3]), "1,2");
shouldBe(log.length, 0);

// And the other way round: this realm makes its protocol observable; the other realm's own loops over its own Arrays do not see it.
Object.prototype.return = function () { log.push("this return " + Object.prototype.toString.call(this)); return {}; };
shouldBe(firstTwo([1, 2, 3]), "1,2");
shouldBe(log.join("|"), "this return [object Array Iterator]");
log.length = 0;
let fresh = createGlobalObject();
fresh.log = log;
let freshFirstTwo = fresh.eval("(function (array) { let seen = []; for (let x of array) { seen.push(x); if (seen.length === 2) break; } return seen.join(); })");
let freshArray = fresh.eval("[5, 6, 7]");
for (let i = 0; i < testLoopCount; i++)
    shouldBe(freshFirstTwo(freshArray), "5,6");
shouldBe(log.length, 0);
delete Object.prototype.return;
