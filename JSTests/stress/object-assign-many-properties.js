function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error("bad value: " + actual + " expected: " + expected);
}

function shouldBeFast(object) {
    if (/Dictionary/.test(describe(object)))
        throw new Error("unexpected dictionary: " + describe(object));
}

function makeSource(count, salt) {
    let body = "return {";
    for (let i = 0; i < count; ++i)
        body += "p" + i + ": " + (i + salt) + ",";
    body += "};";
    return new Function(body)();
}

function check(object, count, salt) {
    let keys = Object.keys(object);
    shouldBe(keys.length, count);
    for (let i = 0; i < count; ++i) {
        shouldBe(keys[i], "p" + i);
        shouldBe(object["p" + i], i + salt);
    }
}

function assign(source) { return Object.assign({}, source); }
noInline(assign);
function spread(source) { return { ...source }; }
noInline(spread);
function rest(source) { let { p0, ...others } = source; return others; }
noInline(rest);

let loopCount = Math.min(testLoopCount, 8);
for (let count of [128, 129, 200, 512, 513]) {
    let source = makeSource(count, count);
    shouldBeFast(source);
    for (let iteration = 0; iteration < loopCount; ++iteration) {
        let assigned = assign(source);
        check(assigned, count, count);
        if (count <= 512)
            shouldBeFast(assigned);

        check(spread(source), count, count);

        let others = rest(source);
        shouldBe(Object.keys(others).length, count - 1);
        shouldBe(others.p0, undefined);
        shouldBe(others["p" + (count - 1)], count - 1 + count);
        shouldBeFast(others);

        let target = makeSource(count >> 1, 0);
        Object.assign(target, source);
        check(target, count, count);

        assigned.extra = 42;
        delete assigned.p1;
        shouldBe(assigned.extra, 42);
        shouldBe(assigned.p1, undefined);
        shouldBe(Object.keys(assigned).length, count);
    }
}
