//@ requireOptions("--useExplicitResourceManagement=1")

function shouldBe(actual, expected) {
    if (actual !== expected)
        throw new Error(`Expected ${expected} but got ${actual}`);
}

for (let depth of [5, 8, 9, 16, 17, 20, 32, 64, 128]) {
    eval(
        "{using a=null;" +
        "{using a=null;".repeat(depth - 1) +
        "0;" +
        "}".repeat(depth)
    );
}

{
    let n = 0;
    eval(
        "{using a={[Symbol.dispose](){n++}};" +
        "{using a={[Symbol.dispose](){n++}};".repeat(19) +
        "0;" +
        "}".repeat(20)
    );
    shouldBe(n, 20);
}

{
    let order = [];
    eval(
        "{using a={[Symbol.dispose](){order.push(0)}};" +
        Array.from({length: 15}, (_, i) =>
            `{using a={[Symbol.dispose](){order.push(${i + 1})}};`
        ).join("") +
        "0;" +
        "}".repeat(16)
    );
    shouldBe(order.join(","), "15,14,13,12,11,10,9,8,7,6,5,4,3,2,1,0");
}

for (let depth = 1; depth <= 20; depth++) {
    let n = 0;
    let code = "";
    for (let i = 0; i < depth; i++)
        code += `{using a${i}={[Symbol.dispose](){n++}};using b${i}={[Symbol.dispose](){n++}};`;
    code += "0;" + "}".repeat(depth);
    eval(code);
    shouldBe(n, depth * 2);
}

(async () => {
    for (let depth of [8, 9, 16, 20]) {
        let n = 0;
        let code = "(async()=>{";
        for (let i = 0; i < depth; i++)
            code += `{await using a${i}={[Symbol.asyncDispose](){n++}};`;
        code += "0;" + "}".repeat(depth) + "})()";
        await eval(code);
        shouldBe(n, depth);
    }
})().then(() => {}, e => { print(e); throw e; });
drainMicrotasks();
