const icCount = 100;
const structCount = 16;

let body = "var x = 0;\n";
for (let i = 0; i < icCount; i++)
    body += "x += o.p;\n";
body += "return x;\n";

let objs = [];
for (let i = 0; i < structCount; i++) {
    let o = {};
    o["k" + i] = i;
    o.p = 1;
    objs.push(o);
}

let f = new Function("o", body);

for (let j = 0; j < 130; j++)
    f(objs[j % structCount]);

for (let j = 0; j < 100000; j++)
    f(objs[j % structCount]);

f(42);
