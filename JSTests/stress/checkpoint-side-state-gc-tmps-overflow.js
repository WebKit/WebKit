//@ requireOptions("--useConcurrentGC=false")

var v2 = 2190736854 + 1;
var v3 = v2 + Object;
var v4 = v3;
var v7 = 0;
do {
    function v8(v9,v10,v11,v12,v13) {
        try {
            var v14 = v13();
        } catch(v16) {
        }
        var v18 = gc();
        if (v18) {
            } else {
            }
        var v20 = v9;
        do {
            var v21 = v20 + 1;
            v20 = v21;
        } while (v20 < 4);
    }
    var v24 = new Int32Array();
    var v25 = v8(...v4);
    for (var v29 = -1024; v29 < 100; v29++) {
    }
    var v30 = v7 + 1;
    v7 = v30;
} while (v7 < 10000);
gc();
