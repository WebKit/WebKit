function test() {
    var re = /./;
    try {
        '/./'.endsWith(re);
    } catch(e){
        re[Symbol.match] = false;
        return '/./'.endsWith(re);
    }
}

if (!test())
    throw new Error("Test failed");
