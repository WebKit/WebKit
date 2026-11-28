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

const GeneratorFunction = (function* () { }).constructor;

const manyBlocks = new GeneratorFunction("a", "b", `
    let first = b + 1, last = b + 2, count = 0;
    yield;
    for (;;) {
        try {
            ${Array.from({ length: 53 }, (_, i) => `if (a === ${i}) { if (count++) return last + ${i}; ${i & 1 ? "continue" : "throw first"}; }`).join("\n")}
            return count;
        } catch (e) {
            yield;
            if (e !== first)
                throw new Error("bad exception");
        }
    }
`);

const manyCases = new GeneratorFunction("a", "b", `
    ${Array.from({ length: 53 }, (_, i) => `let x${i} = b + ${i};`).join("\n")}
    yield;
    switch (a) {
    ${Array.from({ length: 53 }, (_, i) => `case ${(i * 7) % 53}: ${i % 3 ? `return x${i};` : ""}`).join("\n")}
    }
    return -1;
`);

for (let i = 0; i < 53; ++i) {
    shouldBe(run(manyBlocks(i, 10000)), 10002 + i);
    shouldBe(run(manyCases((i * 7) % 53, 10000)), 10000 + (i % 3 ? i : i + 1));
}
shouldBe(run(manyBlocks(-1, 10000)), 0);
shouldBe(run(manyCases(-1, 10000)), -1);
