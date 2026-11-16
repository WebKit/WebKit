function splitLines(text)
{
    return text.split(/\r?\n/);
}
noInline(splitLines);

const lines = [];
for (let i = 0; i < 200; ++i)
    lines.push("line " + i + " " + "x".repeat((i * 7) % 60));
const lf = lines.join("\n");
const crlf = lines.join("\r\n");

let count = 0;
for (let i = 0; i < 1e4; ++i)
    count += splitLines(lf).length + splitLines(crlf).length;
if (count !== 4e6)
    throw new Error("bad count: " + count);
