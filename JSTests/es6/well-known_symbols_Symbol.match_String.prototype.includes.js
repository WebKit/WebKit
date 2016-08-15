function test() {
    var re = /./;
    try {
        '/./'.includes(re);
    } catch(e){
        re[Symbol.match] = false;
        return '/./'.includes(re);
    }
}

if (!test())
    throw new Error("Test failed");
