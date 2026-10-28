const object = { a: 1, b: 2, c: 3, d: 4, e: 5, f: 6, g: 7, h: 8 };

function test(object) {
    return Object.entries(object);
}
noInline(test);

for (let i = 0; i < 1e6; i++)
    test(object);
