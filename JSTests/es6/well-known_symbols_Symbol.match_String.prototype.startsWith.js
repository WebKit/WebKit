function test() {
    var re = /./;
    try {
        '/./'.startsWith(re);
    } catch(e){
        re[Symbol.match] = false;
        return '/./'.startsWith(re);
    }
}

if (!test())
    throw new Error("Test failed");
