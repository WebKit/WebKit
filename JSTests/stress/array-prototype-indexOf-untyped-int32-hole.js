function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

function test(array, searchElement) {
    return array.indexOf(searchElement);
}
noInline(test);

const array = new Array(1024);
for (let i = 0; i < array.length; i++) {
    if (i !== 256)
        array[i] = i;
}

for (let i = 0; i < testLoopCount; i++) {
    const odd = i % 2 === 0;
    const searchElement = odd ? undefined : {};
    shouldBe(test(array, searchElement), -1);
}
