//@ runDefault("--exposePrivateIdentifiers=1")
function foo(v) {
    @superSamplerBegin()
    for (let i = 0; i < 1e7; ++i) { v++; }
    @superSamplerEnd();
    return v;
}
noInline(foo);

foo(1);