const table = { "content-type": 1, "x-request-id": 2, "ACCEPT": 3 };

function getLower(name) { return table[name.toLowerCase()]; }
function getUpper(name) { return table[name.toUpperCase()]; }

function test() {
    return getLower("Content-Type") + getLower("X-Request-Id") + getUpper("Accept");
}
noInline(test);

let result = 0;
for (let i = 0; i < 1e6; ++i)
    result += test();
if (result !== 6e6)
    throw new Error("bad result: " + result);
