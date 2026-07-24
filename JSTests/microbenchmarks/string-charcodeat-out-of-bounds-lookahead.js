function scan(string) {
    let count = 0;
    for (let i = 0; i < string.length; i++) {
        const c = string.charCodeAt(i);
        const next = string.charCodeAt(i + 1);
        if (c === 47 && next === 47)
            count += 3;
        count += c & 1;
    }
    return count;
}
noInline(scan);

let string = "";
for (let i = 0; i < 500; i++)
    string += String.fromCharCode(32 + (i * 7) % 95);

let result = 0;
for (let i = 0; i < 1e4; i++)
    result += scan(string);
if (result !== 2480000)
    throw new Error("Bad result: " + result);
