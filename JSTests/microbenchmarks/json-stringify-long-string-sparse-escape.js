// Long 8-bit strings with sparsely distributed characters that require JSON
// escaping (~1.5% density). This is representative of stringifying large text
// payloads such as documents, logs, or chat transcripts where most content is
// plain ASCII prose with occasional newlines and quotes.

function makeParagraph(words, escapeEvery)
{
    let s = "";
    for (let i = 0; i < words; ++i) {
        s += "lorem ipsum dolor sit amet ";
        if (!(i % escapeEvery))
            s += i % 2 ? "\n" : '"';
    }
    return s;
}

const body = {
    version: 1,
    items: [],
};
for (let i = 0; i < 20; ++i) {
    body.items.push({
        id: "item-" + i,
        kind: "text",
        content: makeParagraph(200, 4),
    });
}

const expectedLength = JSON.stringify(body).length;
for (let i = 0; i < 1000; ++i) {
    if (JSON.stringify(body).length !== expectedLength)
        throw new Error("bad result");
}
