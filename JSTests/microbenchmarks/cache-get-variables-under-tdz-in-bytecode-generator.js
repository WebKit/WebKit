//@ runDefault

let script = "(() => {";
for (let i = 0; i < 1000; ++i) {
    script += `let x${i} = ${i};\n`;
}

for (let i = 0; i < 1000; ++i) {
    script += `function f${i}() { return "foo"; }\n`;
}

script += "})();"

let start = Date.now();
for (let i = 0; i < 10; ++i) {
    $.evalScript(`cacheBust = ${Math.random()}; ${script}`);
}

if (false)
    print(Date.now() - start);
