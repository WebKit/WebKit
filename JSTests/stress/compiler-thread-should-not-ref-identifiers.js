//@ slow!
//@ runDefault

for (let j = 0; j < 500; j++) {
    runString(`

let a = Symbol('a');
let o = { [a]: 1 };

for (let i = 0; i < testLoopCount; ++i) {
    Object.getOwnPropertySymbols(o);
    o[a];
}
`);
}
