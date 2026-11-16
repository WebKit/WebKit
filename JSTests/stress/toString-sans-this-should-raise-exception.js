function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}

function toStringRipper(a) {
    const toStringRipped = a.toString;
    return toStringRipped();
}

toStringRipper({toString:Symbol})

let errCount = 0
for (let i = 0; i < testLoopCount; i++) {
    try {
        toStringRipper(i)
    } catch (e) {
        errCount++
    }
}

shouldBe(errCount, testLoopCount)
