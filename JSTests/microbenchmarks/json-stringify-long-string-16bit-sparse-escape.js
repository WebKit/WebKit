// Long 16-bit strings (non-Latin1 content) with sparsely distributed escape
// characters. Exercises the 16-bit SIMD path including the surrogate check.

function makeParagraph(words, escapeEvery)
{
    let s = "";
    for (let i = 0; i < words; ++i) {
        s += "\u3042\u3044\u3046\u3048\u304a\u304b\u304d\u304f\u3051\u3053 ";
        if (!(i % escapeEvery))
            s += "\n";
    }
    return s;
}

const body = {
    items: [],
};
for (let i = 0; i < 20; ++i)
    body.items.push({ id: i, content: makeParagraph(300, 5) });

const expectedLength = JSON.stringify(body).length;
for (let i = 0; i < 1000; ++i) {
    if (JSON.stringify(body).length !== expectedLength)
        throw new Error("bad result");
}
