function test() {
    const map = new Map();
    var iterator = map[Symbol.iterator]();
    const proto = Object.getPrototypeOf(iterator);
    try{
        map.get(...map, ...proto);
    }catch(e){}
}
for (var i=0;i<testLoopCount;i++)
    test();
