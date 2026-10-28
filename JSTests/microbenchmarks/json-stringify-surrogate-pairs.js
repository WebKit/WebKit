function makeBody(words)
{
    let s = "";
    for (let i = 0; i < words; ++i) {
        s += "lorem ipsum \u65E5\u672C\u8A9E dolor sit amet ";
        if (!(i % 4))
            s += i % 8 ? "\u{1F680} " : "\u{1F642} ";
    }
    return s;
}

const posts = [];
for (let i = 0; i < 20; ++i)
    posts.push({ id: i, title: "post " + i, body: makeBody(20 + i) });

const expectedLength = JSON.stringify(posts).length;
for (let i = 0; i < 5000; ++i) {
    if (JSON.stringify(posts).length !== expectedLength)
        throw new Error("bad result");
}
