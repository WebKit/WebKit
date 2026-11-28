function shouldBe(actual, expected)
{
    if (actual !== expected)
        throw new Error(`expected ${expected} but got ${actual}`);
}

function run(generator)
{
    for (;;) {
        const { value, done } = generator.next();
        if (done)
            return value;
    }
}

function* denseSwitch(v)
{
    let x = v + 1, y = v + 2, z = v + 3;
    yield;
    switch (v) {
    case 0:
    case 1:
    case 2:
        return x;
    case 3:
    case 7:
        return y;
    default:
        return z;
    }
}

function* sparseSwitch(v)
{
    let x = v + 1, y = v + 2, z = v + 3;
    yield;
    switch (v) {
    case 0:
    case 100000:
        return x;
    case -100000:
    case 12345678:
        return y;
    }
    return z;
}

function* fallThroughSwitch(v)
{
    let x = v + 1, y = v + 2, z = v + 3, result = 0;
    yield;
    switch (v) {
    case 5:
        result += x;
    default:
        result += y;
    case 1:
        yield;
        result += z;
        break;
    case 3:
        break;
    }
    return result;
}

function* emptySwitch(v)
{
    let x = v + 1;
    yield;
    switch (v) {
    case 0:
    case 1:
    case 2:
    }
    return x;
}

function* charSwitch(v)
{
    let x = 1, y = 2, z = 3;
    yield;
    switch ("abcdefgh"[v & 7]) {
    case "a":
    case "c":
        return x + v;
    case "b":
    case "g":
        return y + v;
    default:
        return z + v;
    }
}

function* stringSwitch(v)
{
    let x = 1, y = 2, z = 3;
    yield;
    switch ("key" + (v & 7)) {
    case "key6":
    case "key1":
        return x + v;
    case "key0":
    case "key7":
        return y + v;
    default:
        return z + v;
    }
}

function* throwInTry(v)
{
    let x = v + 1, y = v + 2, z = v + 3;
    yield;
    try {
        try {
            if (v & 1)
                throw x;
            if (v & 2)
                throw y;
        } catch (e) {
            yield;
            if (e === x)
                throw z;
            return y;
        } finally {
            yield;
            z += 10;
        }
    } catch (e) {
        yield;
        return e + z;
    }
    return z;
}

function* loops(v)
{
    let x = 0, y = v, z = 0;
    outer: for (let i = 0; i < 4; ++i) {
        for (let j = 0; j < 3; ++j) {
            yield;
            if (i === 1 && j === 1)
                continue outer;
            if (i === 3)
                break outer;
            x += y;
            while (z < j) {
                yield;
                ++z;
            }
        }
        x += z;
    }
    return x + z;
}

function shouldThrow(v)
{
    let x = v + 1, y = v + 2;
    switch (v & 3) {
    case 0:
        throw x;
    case 1:
        throw y;
    case 2:
        try {
            throw x;
        } catch (e) {
            return e + y;
        }
    }
    return x;
}

for (let i = 0; i < testLoopCount; i += 4) {
    const v = i % 9;
    shouldBe(run(denseSwitch(v)), v <= 2 ? v + 1 : v === 3 || v === 7 ? v + 2 : v + 3);
    const s = [0, 100000, -100000, 12345678, 4][i % 5];
    shouldBe(run(sparseSwitch(s)), i % 5 < 2 ? s + 1 : i % 5 < 4 ? s + 2 : s + 3);
    shouldBe(run(fallThroughSwitch(v)), v === 5 ? 21 : v === 1 ? 4 : v === 3 ? 0 : 2 * v + 5);
    shouldBe(run(emptySwitch(v)), v + 1);
    shouldBe(run(charSwitch(v)), v + [1, 2, 1, 3, 3, 3, 2, 3, 1][v]);
    shouldBe(run(stringSwitch(v)), v + [2, 1, 3, 3, 3, 3, 1, 2, 2][v]);
    shouldBe(run(throwInTry(v)), v & 1 ? (v + 3) + (v + 13) : v & 2 ? v + 2 : v + 13);
    shouldBe(run(loops(v)), 7 * v + 6);

    let result;
    try {
        result = shouldThrow(v);
    } catch (e) {
        result = -e;
    }
    shouldBe(result, [-(v + 1), -(v + 2), 2 * v + 3, v + 1][v & 3]);
}
