// Long strings where every character requires JSON escaping. This is the
// pathological case for any "scan for the next escape" strategy and should
// not regress compared to per-character processing.

const chunk = "\n\t\"\\".repeat(3000);
const body = { a: chunk, b: chunk, c: chunk };

const expectedLength = JSON.stringify(body).length;
for (let i = 0; i < 2000; ++i) {
    if (JSON.stringify(body).length !== expectedLength)
        throw new Error("bad result");
}
