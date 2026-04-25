//@ runDefault("--watchdog=200", "--watchdog-exception-ok")
function main() {
    const v2 = new Uint8Array(7792);
    function v3(v4,v5,...v6) {
        const v9 = createGlobalObject();
        const v11 = function() { return 42; }
        const v15 = {"get":v11};
        const v17 = new Proxy(v9,v15);
        const v18 = v17.foobar;
    }
    const v20 = v2.find(v3);
    gc();
}
noDFG(main);
noFTL(main);
main();
