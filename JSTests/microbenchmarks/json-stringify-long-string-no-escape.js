// Long 8-bit strings containing no characters that require JSON escaping.
// Exercises the pure bulk-copy path of appendEscapedJSONStringContent.

const chunk = "abcdefghijklmnopqrstuvwxyz0123456789 ".repeat(300);
const body = {
    items: [],
};
for (let i = 0; i < 20; ++i)
    body.items.push({ id: i, content: chunk });

const expectedLength = JSON.stringify(body).length;
for (let i = 0; i < 1000; ++i) {
    if (JSON.stringify(body).length !== expectedLength)
        throw new Error("bad result");
}
