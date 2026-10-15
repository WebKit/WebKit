function shouldBe(actual, expected, message) {
    if (actual !== expected)
        throw new Error(message + ": expected " + expected + " but got " + actual);
}

let siteCount = 0;
function makeSite() {
    return new Function(`
        return async function (iterable) {
            try {
                let result = 0;
                for await (let x of iterable)
                    result = result * 10 + x;
                return result;
            } catch (error) {
                return error instanceof TypeError ? "TypeError" : "unexpected " + error;
            }
        };
        // ${siteCount++}`)();
}

async function* asyncGenerator() { yield 1; yield 2; }
function* syncGenerator() { yield 3; yield 4; }
let AsyncGeneratorPrototype = Object.getPrototypeOf(asyncGenerator.prototype);
let AsyncIteratorPrototype = Object.getPrototypeOf(AsyncGeneratorPrototype);

function countingNext() {
    let i = 0;
    return () => Promise.resolve(i < 2 ? { value: ++i + 4, done: false } : { value: undefined, done: true });
}

let kinds = {
    asyncGenerator: [() => asyncGenerator(), 12],
    sync: [() => [3, 4], 34],
    generic: [() => ({ [Symbol.asyncIterator]() { return { next: countingNext() }; } }), 56],
};

let strangers = [
    [() => Object.create(AsyncGeneratorPrototype), "TypeError"],
    [() => Object.setPrototypeOf(syncGenerator(), AsyncGeneratorPrototype), "TypeError"],
    [() => Object.setPrototypeOf([1, 2], AsyncGeneratorPrototype), "TypeError"],
    [() => Object.assign(Object.create(AsyncIteratorPrototype), { next: countingNext() }), 56],
];

let sites = [
    ["asyncGenerator", "sync"],
    ["asyncGenerator", "generic"],
];

async function main() {
    for (let names of sites) {
        let site = makeSite();
        for (let i = 0; i < testLoopCount; ++i) {
            let name = names[i % names.length];
            shouldBe(await site(kinds[name][0]()), kinds[name][1], names + " with " + name);
        }
        for (let name in kinds)
            shouldBe(await site(kinds[name][0]()), kinds[name][1], names + " with " + name);
        for (let i = 0; i < strangers.length; ++i)
            shouldBe(await site(strangers[i][0]()), strangers[i][1], names + " with stranger " + i);
    }
}

let failure = null;
let finished = false;
main().then(() => { finished = true; }, error => { failure = error; });
drainMicrotasks();
if (failure)
    throw failure;
shouldBe(finished, true, "finished");
