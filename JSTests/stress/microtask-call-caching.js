// MicrotaskCall caching: verify correctness for promise chains using the same handler function.
let results = [];
const handler = (v) => { results.push(v); };

let promises = [];
for (let i = 0; i < 1000; i++)
    promises.push(Promise.resolve(i).then(handler));

Promise.all(promises).then(() => {
    if (results.length !== 1000)
        throw new Error("Wrong count: " + results.length);
    results.sort((a, b) => a - b);
    for (let i = 0; i < 1000; i++) {
        if (results[i] !== i)
            throw new Error("Wrong value at " + i + ": " + results[i]);
    }
});
