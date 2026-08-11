function test() {
    let count = 0;
    for (let i = 0; i < 1e4; ++i)
        count += new Intl.DurationFormat("en").format !== undefined;
    return count;
}

const result = test();
if (result !== 1e4)
    throw new Error("Bad result: " + result);
