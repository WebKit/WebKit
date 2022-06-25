
var object = {
    get hello() {
        return 42;
    }
};

function ok() {
    var value = 'hello';
    if (object[value] + 20 !== 62)
        throw new Error();
}
noInline(ok);

for (var i = 0; i < 10000; ++i)
    ok();
