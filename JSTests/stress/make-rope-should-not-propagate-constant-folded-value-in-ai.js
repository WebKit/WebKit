//@ runDefault("--forceEagerCompilation=1")
let string = 'a';
let array = []

function test() {
    let x = [][array.length];
    let y = x ? x + '' : '';
    let z = string + y;
    let za = z + 'a';
    [].join(za);
}

for (let i=0; i< 1e4; i++)
    test();
