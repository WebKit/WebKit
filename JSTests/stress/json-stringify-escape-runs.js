function reference(s) {
    let out = '"';
    for (let i = 0; i < s.length; ++i) {
        let c = s.charCodeAt(i);
        if (c === 0x22) out += '\\"';
        else if (c === 0x5c) out += '\\\\';
        else if (c === 8) out += '\\b';
        else if (c === 9) out += '\\t';
        else if (c === 10) out += '\\n';
        else if (c === 12) out += '\\f';
        else if (c === 13) out += '\\r';
        else if (c < 0x20) out += '\\u' + c.toString(16).padStart(4, '0');
        else out += s[i];
    }
    return out + '"';
}
let seed = 7;
function rand(n) { seed = (seed * 1103515245 + 12345) & 0x7fffffff; return seed % n; }
let specials = ['"', '\\', '\n', '\t', '\x00', '\x1f', '\x7f', '\xff', '\r', '\b', '\f', '\x01'];
for (let iter = 0; iter < 3000; ++iter) {
    let len = rand(4) === 0 ? rand(300) : rand(40);
    let chars = [];
    let density = rand(5);
    for (let i = 0; i < len; ++i) {
        if (density && rand(density * 8) === 0) chars.push(specials[rand(specials.length)]);
        else chars.push(String.fromCharCode(0x20 + rand(0xff - 0x20)));
    }
    let s = chars.join('');
    let expected = reference(s);
    let got = JSON.stringify(s);
    if (got !== expected) throw new Error('mismatch ' + iter + ' ' + escape(s));
    let arr = JSON.stringify([s, { k: s }, s]);
    if (arr !== '[' + expected + ',{"k":' + expected + '},' + expected + ']') throw new Error('mismatch in container ' + iter);
    let big = JSON.stringify({ a: s.repeat(50), b: [s] });
    if (big !== '{"a":' + reference(s.repeat(50)) + ',"b":[' + expected + ']}') throw new Error('big mismatch');
}
for (let special of specials) {
    for (let length of [1, 15, 16, 17, 31, 32, 33, 64, 100]) {
        let dense = special.repeat(length);
        if (JSON.stringify(dense) !== reference(dense)) throw new Error('dense mismatch');
        let alternating = ('a' + special).repeat(length);
        if (JSON.stringify(alternating) !== reference(alternating)) throw new Error('alternating mismatch');
    }
}
