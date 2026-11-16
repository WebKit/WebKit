// Stresses SetFunctionName for anonymous arrow function class fields.
class ArrowFunctionFields {
    render = (...args) => args;
    setLayout = (l) => l;
    getLayout = () => 0;
    setRenderer = (r) => r;
    header = (n, v) => v;
    status = (s) => s;
    set = (k, v) => v;
    get = (k) => k;
    newResponse = (...a) => a;
    body = (d) => d;
    text = (t) => t;
    json = (o) => o;
    html = (h) => h;
    redirect = (l) => l;
    notFound = () => null;
}

function bench(testClass) {
    var instance;
    for (var i = 0; i < 1e5; i++)
        instance = new testClass();
    return instance;
}
noInline(bench);

bench(ArrowFunctionFields);
