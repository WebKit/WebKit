const regexp = /(\d+)-(\d+)-(\d+)/d;

function test(string) {
    return regexp.exec(string);
}
noInline(test);

for (let i = 0; i < 1e6; i++)
    test("date: 2026-10-27");
