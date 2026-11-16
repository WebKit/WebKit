function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`bad value: ${String(actual)}, expected: ${String(expected)}`);
}

function Pair(first, second)
{
    this.first = first;
    this.second = second;
}

const rareStride = Math.max(1, Math.floor(testLoopCount / 100));

function callsInElements(a)
{
    return Reflect.construct(Pair, [Math.max(a, 0), Reflect.construct(Pair, [new Pair(a, a + 1), Math.min.apply(null, [a, a + 2])])], Pair);
}
noInline(callsInElements);

function conditionalElements(a, flag)
{
    return Reflect.construct(Pair, [flag ? Reflect.construct(Pair, [a, 1]) : a, flag || Reflect.construct(Pair, [2, a])]);
}
noInline(conditionalElements);

function tryInElements(a)
{
    let results = [];
    for (let target of [Pair, undefined]) {
        try {
            results.push(Reflect.construct(target, [a, results.length]));
        } catch (error) {
            results.push(error);
        } finally {
            results.push("finally");
        }
    }
    return results;
}
noInline(tryInElements);

function closesOverElements(a)
{
    let captured = a;
    let object = Reflect.construct(Pair, [() => captured, captured = a + 1]);
    return [object.first(), object.second];
}
noInline(closesOverElements);

async function awaitsInElements(a)
{
    return Reflect.construct(Pair, [await a, await Promise.resolve(a + 1)], await Pair);
}

let awaited = 0;
let expectedAwaited = 0;
for (let i = 0; i < testLoopCount; ++i) {
    let object = callsInElements(i);
    shouldBe(object.first, i);
    shouldBe(object.second.first.first, i);
    shouldBe(object.second.first.second, i + 1);
    shouldBe(object.second.second, i);

    object = conditionalElements(i, true);
    shouldBe(object.first.first, i);
    shouldBe(object.second, true);
    object = conditionalElements(i, false);
    shouldBe(object.first, i);
    shouldBe(object.second.second, i);

    let results = tryInElements(i);
    shouldBe(results.length, 4);
    shouldBe(results[0].first, i);
    shouldBe(results[0].second, 0);
    shouldBe(results[1], "finally");
    shouldBe(results[2] instanceof TypeError, true);
    shouldBe(results[3], "finally");

    let [first, second] = closesOverElements(i);
    shouldBe(first, i + 1);
    shouldBe(second, i + 1);

    if (i % rareStride)
        continue;
    ++expectedAwaited;
    awaitsInElements(i).then((object) => {
        shouldBe(object.first, i);
        shouldBe(object.second, i + 1);
        ++awaited;
    });
}
drainMicrotasks();
shouldBe(awaited, expectedAwaited);
