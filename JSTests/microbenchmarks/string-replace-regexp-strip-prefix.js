function test(url)
{
    return url.replace(/^https?:\/\//, "");
}
noInline(test);

const urls = [];
for (let k = 0; k < 64; k++)
    urls.push((k & 1 ? "https://" : "http://") + "host" + k + ".example.com/path/to/resource/" + k + "?query=" + (k * 7));

let result;
for (let i = 0; i < 2000000; ++i)
    result = test(urls[i & 63]);

if (test("https://a.example/b") !== "a.example/b")
    throw new Error("bad result: " + test("https://a.example/b"));
