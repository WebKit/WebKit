function map(e,t,i){
    var r;
    var o=0;
    var s=e.length;
    var a=n(e);
    var l=[];
    if(a) {
        for(;s>o;o++){
            r=t(e[o],o,i);
            null!=r&&l.push(r);
        }
    }
    return l
}

function n(e){
    var t="length" in e && e.length;
    var n = typeof e;
    return "function" === n || t;
}
noInline(map);

function one() { return 1; }

for(i = 0; i < 20000; i++) {
    map([], one, "");
}

// This should throw an exception but it shouldn't crash.
try {
    map("", one, "");
} catch (e) { }
