const names = ["あいう", "かきく", "さしす", "たちつ"];
const items = [];
for (let i = 0; i < 200; ++i) {
    items.push({
        id: i,
        name: names[i % names.length],
        price: i * 3,
        tag: "tag" + (i % 7),
        ok: (i & 1) === 0,
    });
}
const text = JSON.stringify(items);

for (let i = 0; i < 1e4; ++i)
    JSON.parse(text);
