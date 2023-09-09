let o = { "0": "a", 1: "b", p1: 1, p2: 2, p3: 1, p4: 2, p5: 1, p6: 2, p7: 1, p8: 2, "1000": 1000 };
for (let i = 0; i < 1e4; i++) {
    Object.assign({ "-3": -1 }, o);
    Object.assign({}, o);
}
