const objs = [];
for (let j = 0; j < 16; j++) {
    objs.push({
        id: j,
        name: 'user' + j,
        tags: ['alpha', 'beta', 'gamma'],
        nested: { active: true, score: j * 1.5, items: [1, 2, 3, j] },
    });
}

let result = 0;
for (let i = 0; i < 2e6; ++i) {
    const o = objs[i & 15];
    o.id = i;
    result += JSON.stringify(o, null, 2).length;
}
