function shouldBe(actual, expected, msg) {
    if (actual !== expected)
        throw new Error((msg ? msg + ": " : "") + "expected " + expected + " but got " + actual);
}

function shouldThrow(func, errorMessage) {
    var errorThrown = false;
    try {
        func();
    } catch (e) {
        errorThrown = true;
        if (String(e) !== errorMessage)
            throw new Error("expected error '" + errorMessage + "' but got '" + e + "'");
    }
    if (!errorThrown)
        throw new Error("expected to throw");
}

// Empty Map / Set: forEach should not invoke the callback at all.
function testEmpty() {
    var calls = 0;
    new Map().forEach(() => calls++);
    new Set().forEach(() => calls++);
    shouldBe(calls, 0, "empty");
}
noInline(testEmpty);

// Delete a not-yet-visited entry inside the callback: the deleted entry must be skipped.
function testDeleteDuringMap() {
    var m = new Map();
    for (var i = 0; i < 10; i++) m.set(i, i * 10);
    var visited = [];
    m.forEach((v, k) => { visited.push(k); if (k === 3) m.delete(5); });
    shouldBe(visited.join(","), "0,1,2,3,4,6,7,8,9", "delete during Map.forEach");
}
noInline(testDeleteDuringMap);

function testDeleteDuringSet() {
    var s = new Set();
    for (var i = 0; i < 10; i++) s.add(i);
    var visited = [];
    s.forEach((v) => { visited.push(v); if (v === 3) s.delete(5); });
    shouldBe(visited.join(","), "0,1,2,3,4,6,7,8,9", "delete during Set.forEach");
}
noInline(testDeleteDuringSet);

// Delete the entry currently being visited: should not affect already-yielded value.
function testDeleteSelf() {
    var m = new Map();
    for (var i = 0; i < 5; i++) m.set(i, i);
    var visited = [];
    m.forEach((v, k) => { visited.push(k); m.delete(k); });
    shouldBe(visited.join(","), "0,1,2,3,4", "delete self");
    shouldBe(m.size, 0, "delete self size");
}
noInline(testDeleteSelf);

// Add entries inside the callback: new entries must be visited (in insertion order).
function testAddDuring() {
    var m = new Map();
    for (var i = 0; i < 3; i++) m.set(i, i);
    var visited = [];
    m.forEach((v, k) => { visited.push(k); if (k === 1) { m.set(3, 3); m.set(4, 4); } });
    shouldBe(visited.join(","), "0,1,2,3,4", "add during Map.forEach");
}
noInline(testAddDuring);

// Trigger a rehash inside the callback: iteration must continue on the new (rehashed) table.
function testRehashDuring() {
    var m = new Map();
    for (var i = 0; i < 4; i++) m.set(i, i);
    var visited = [];
    m.forEach((v, k) => {
        visited.push(k);
        if (k === 1) {
            for (var j = 100; j < 130; j++) m.set(j, j);
        }
    });
    shouldBe(visited.length, 34, "rehash during Map.forEach count");
    shouldBe(visited[0], 0);
    shouldBe(visited[1], 1);
    shouldBe(visited[2], 2);
    shouldBe(visited[3], 3);
    shouldBe(visited[4], 100);
    shouldBe(visited[33], 129);
}
noInline(testRehashDuring);

// Trigger multiple rehashes that produce a chain of obsolete tables.
function testMultiRehashDuring() {
    var m = new Map();
    for (var i = 0; i < 4; i++) m.set(i, i);
    var visited = [];
    var rehashCount = 0;
    m.forEach((v, k) => {
        visited.push(k);
        if (rehashCount < 5) {
            rehashCount++;
            var base = 1000 * rehashCount;
            for (var j = 0; j < 30; j++) m.set(base + j, base + j);
        }
    });
    shouldBe(visited.length, 4 + 5 * 30, "multi rehash count");
    shouldBe(visited[0], 0);
    shouldBe(visited[3], 3);
    shouldBe(visited[4], 1000);
    shouldBe(visited[visited.length - 1], 5029);
}
noInline(testMultiRehashDuring);

// Clear inside the callback: iteration must stop after the current entry.
function testClearDuring() {
    var m = new Map();
    for (var i = 0; i < 10; i++) m.set(i, i);
    var visited = [];
    m.forEach((v, k) => { visited.push(k); if (k === 3) m.clear(); });
    shouldBe(visited.join(","), "0,1,2,3", "clear during Map.forEach");
}
noInline(testClearDuring);

function testClearDuringSet() {
    var s = new Set();
    for (var i = 0; i < 10; i++) s.add(i);
    var visited = [];
    s.forEach((v) => { visited.push(v); if (v === 3) s.clear(); });
    shouldBe(visited.join(","), "0,1,2,3", "clear during Set.forEach");
}
noInline(testClearDuringSet);

// Heavy tombstone density (delete-heavy) before forEach: tombstone skip loop runs many iterations.
function testTombstoneHeavy() {
    var m = new Map();
    for (var i = 0; i < 100; i++) m.set(i, i);
    for (var i = 1; i < 100; i++) m.delete(i);  // keep only entry 0
    var visited = [];
    m.forEach((v, k) => visited.push(k));
    shouldBe(visited.join(","), "0", "tombstone heavy");
}
noInline(testTombstoneHeavy);

// Callback throws: exception must propagate, no crash.
function testCallbackThrows() {
    var m = new Map();
    for (var i = 0; i < 5; i++) m.set(i, i);
    var visited = [];
    shouldThrow(() => {
        m.forEach((v, k) => { visited.push(k); if (k === 2) throw new Error("boom"); });
    }, "Error: boom");
    shouldBe(visited.join(","), "0,1,2", "callback throws");
}
noInline(testCallbackThrows);

// Delete then re-add the same key inside the callback: re-added key must be visited again.
function testDeleteReAdd() {
    var m = new Map();
    for (var i = 0; i < 4; i++) m.set(i, i);
    var visited = [];
    var done = false;
    m.forEach((v, k) => { visited.push(k); if (k === 1 && !done) { done = true; m.delete(0); m.set(0, 0); } });
    shouldBe(visited.join(","), "0,1,2,3,0", "delete then re-add");
}
noInline(testDeleteReAdd);

// thisArg is passed to the callback.
function testThisArg() {
    var m = new Map([[1, "a"], [2, "b"]]);
    var thisArg = { tag: 42 };
    var got = [];
    m.forEach(function (v, k) { got.push(this.tag); }, thisArg);
    shouldBe(got.join(","), "42,42", "thisArg");
}
noInline(testThisArg);

for (var iter = 0; iter < 1e3; iter++) {
    testEmpty();
    testDeleteDuringMap();
    testDeleteDuringSet();
    testDeleteSelf();
    testAddDuring();
    testRehashDuring();
    testMultiRehashDuring();
    testClearDuring();
    testClearDuringSet();
    testTombstoneHeavy();
    testCallbackThrows();
    testDeleteReAdd();
    testThisArg();
}
