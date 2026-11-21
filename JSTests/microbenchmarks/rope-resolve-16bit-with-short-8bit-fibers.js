function build()
{
    let string = "\u3042";
    for (let i = 0; i < fibers.length; ++i)
        string += fibers[i];
    return string;
}
noInline(build);

const alphabet = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ\xe9\xff";
const fibers = [];
for (let i = 0; i < 40; ++i)
    fibers.push(alphabet.slice(0, 1 + (i * 7) % 40), ",", "\"", "\xe9:");

function check(string)
{
    let index = 0;
    if (string.charCodeAt(index++) !== 0x3042)
        throw new Error("Bad first character");
    for (const fiber of fibers) {
        for (let i = 0; i < fiber.length; ++i) {
            if (string.charCodeAt(index++) !== fiber.charCodeAt(i))
                throw new Error("Bad character at " + (index - 1));
        }
    }
    if (index !== string.length)
        throw new Error("Bad length: " + string.length);
}
check(build());

let last = 0;
for (let i = 0; i < 5e3; ++i) {
    const string = build();
    last += string.charCodeAt(string.length - 1);
}
if (last !== 5e3 * ":".charCodeAt(0))
    throw new Error("Bad result: " + last);
