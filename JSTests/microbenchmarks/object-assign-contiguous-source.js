let o = { "0": "a", "1": "a", "2": "a", "3": "a", "4": "a", "5": "a", "6": "a", "7": "a", "8": "a", };
for (let i = 0; i < 1e4; i++) {
    Object.assign({}, o);
    Object.assign({ "0": -1 }, o);
}
