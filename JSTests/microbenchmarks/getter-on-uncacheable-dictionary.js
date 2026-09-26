const inner = {};
const names = [];
for (let i = 0; i < 150; ++i) {
    names.push("name" + i);
    inner["name" + i] = function () { return i; };
}

let source = names.map((name) => `exports.${name}`).join(" = ") + " = void 0;\n";
for (const name of names)
    source += `Object.defineProperty(exports, "${name}", { enumerable: true, get: function () { return inner.${name}; } });\n`;

const exports = {};
new Function("exports", "inner", source)(exports, inner);

function test(object) {
    let result = 0;
    for (let i = 0; i < 1e6; ++i)
        result += (0, object.name3)() + (0, object.name120)();
    return result;
}
noInline(test);

if (test(exports) !== 123e6)
    throw new Error("bad result");
