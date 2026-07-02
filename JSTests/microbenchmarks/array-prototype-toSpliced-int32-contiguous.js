function test(array, start, del, insert) {
    return array.toSpliced(start, del, ...insert);
}
noInline(test);

function shouldBe(a, b) {
    if (a !== b)
        throw new Error(`Expected ${b} but got ${a}`);
}

const src    = Array.from({ length: 1024 }, (_, i) => i | 0);
const insert = Array.from({ length: 32   }, (_, i) => ({ v: -i }));

let result;
for (let i = 0; i < 1e5; ++i)
    result = test(src, 496, 32, insert);

shouldBe(result.length, 1024);
