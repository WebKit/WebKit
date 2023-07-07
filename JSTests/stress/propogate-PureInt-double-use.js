function opt(v)
{
    let a = (-1 >>> v)
    let b = (a + -0.2)
    let c = b | 0
    return c
}
noInline(opt)

function o(v) {
    opt(v)
}
noInline(o)

function main() {
    for (let i = 0; i < 10000; i++)
        o(0)
    for (let i = 0; i < 10000; ++i)
     opt(32)
    if (opt(32) != -2)
        throw "wrong value"
}
noInline(main)
main()