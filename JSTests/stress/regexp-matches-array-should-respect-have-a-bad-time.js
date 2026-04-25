function F0(){
    class C4{}
    function f5(a6,a7,a8){
        return {__proto__:a7}
    }
    f5(C4,C4)
    Object.defineProperty(C4, 8, {configurable:true, get: f5, set: f5})
}
for(let v11=0;v11<5;v11++){
    v14 = ("size").match(/e/g)
    new F0()
    v14[41530] = v14
}
