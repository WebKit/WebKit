function f() {
    const re = /a/y;
    return re.test(re);
}
noInline(f);

for (let i = 0; i < testLoopCount; ++i)
    f();
