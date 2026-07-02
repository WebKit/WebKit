
let chars = ["鴬", "𐊧", "Ϊ"];
let continueChars =  [unescape("\u0311"), String.fromCharCode(...[0xDB40, 0xDD96])];

let o = { };
for (let c of chars) {
    eval(`var ${c};`);
    eval(`function foo() { var ${c} }`);
    eval(`o.${c}`);
}

function throwsSyntaxError(string) {
    try {
        eval(string);
    } catch (e) {
        if (!(e instanceof SyntaxError))
            throw new Error(string);
        return;
    }
    throw new Error(string);
}

for (let c of continueChars) {
    throwsSyntaxError(`var ${c}`);
    throwsSyntaxError(`function foo() { var ${c} }`);
    throwsSyntaxError(`o.${c}`);
    eval(`var ${("a" + c)}`);
    eval(`o.${"a" + c}`);

}
