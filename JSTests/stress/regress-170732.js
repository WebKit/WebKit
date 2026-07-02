//@ runDefault("--exception=SyntaxError")
async function f() {
    await async()=>{}
}
