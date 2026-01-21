function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error('bad value: ' + actual);
}


var map = new Map;
for (var i = 0; i < 100; ++i)
    map.set(i, i);

function test(map) {
    var result = 0;
    let hasThown = false;
    try {
        map.forEach((a, b) => {
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
    shouldBe(test(map), 11);
