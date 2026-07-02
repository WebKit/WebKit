function throwing()
{
    throw new Error('ok');
}
noInline(throwing);

function test()
{
    throwing();
}
noInline(test);

function calling()
{
    test.bind(null)();
}
noInline(calling);

var errors = [];
for (var i = 0; i < 1000; ++i) {
    try {
        calling();
    } catch(e) {
        errors.push(e);
    }
}
fullGC();
