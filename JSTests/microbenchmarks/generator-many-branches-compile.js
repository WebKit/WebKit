//@ skip if $memoryLimited
const GeneratorFunction = (function* () { }).constructor;

let body = "";
for (let i = 0; i < 4000; ++i)
    body += `if (a === ${i}) return b;\n`;
body += "yield b;";

for (let i = 0; i < 3; ++i) {
    const f = new GeneratorFunction("a", "b", `/* ${Math.random()} */ ${body}`);
    const result = f(-1, 42).next();
    if (result.value !== 42 || result.done)
        throw new Error("bad result");
}
