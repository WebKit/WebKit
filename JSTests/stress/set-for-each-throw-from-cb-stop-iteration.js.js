function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var set = new Set;
for (var i = 0; i < 100; ++i)
    set.add(i);

function test(set) {
    let result = 0;
    let hasThown = false;
    try {
        set.forEach((a, b) => {
            if (a > 10) {
                throw new Error('stop iteration!');
            }
            result += 1;
        });
    } catch {
        hasThown = true;
    }

    if (!hasThown)
        throw new Error('not thown expectedly');

    return result;
}
noInline(test);

for (var i = 0; i < testLoopCount; ++i)
    shouldBe(test(set), 11);