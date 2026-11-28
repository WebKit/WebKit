//@ skip if $memoryLimited
const GeneratorFunction = (function* () { }).constructor;

let body = "switch (a) {\n";
for (let i = 0; i < 8000; ++i)
    body += `case ${i}: b += ${i};\n`;
body += "}\nyield b;";

for (let i = 0; i < 3; ++i) {
    const f = new GeneratorFunction("a", "b", `/* ${Math.random()} */ ${body}`);
    const result = f(7998, 1).next();
    if (result.value !== 15998 || result.done)
        throw new Error("bad result");
}
