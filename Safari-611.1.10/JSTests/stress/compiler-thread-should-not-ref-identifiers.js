//@ slow!
//@ runDefault

for (let j = 0; j < 500; j++) {
    runString(`

let a = Symbol('a');
let o = { [a]: 1 };

for (let i = 0; i < 10000; ++i) {
    Object.getOwnPropertySymbols(o);
    o[a];
}
`);
}
