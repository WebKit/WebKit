// Regression test for <rdar://problem/27889416>.

function allocate() {
    var i = 0;
    var array = new Array(17);
    for ( ; i < array.length; i++)
        array[i] = new Uint32Array(0x00040000);
    i = 0;
    var arr = [];
    arr.xxx = "xxx";
    for (var i = 0; i < 1024; i++) {
        arr[i] = new Array(i);
        arr[i].xxx = "xxx " + i
    }

    if (this.gc)
        this.gc();
}

function test() {
    var array = new Array(256);
    var targetReference = [];
    var keepAlive = null;

    for (var x = 0; x < array.length; x++) {
        if (x == array.length / 4) {
            keepAlive = new Array(2047);
            targetReference.shift();
        }

        array[x] = new Array(4095);
    }

    var o = {};
    var l = 0;
    o.toString = function() {
        if (0 == l) {
            keepAlive = null;
            targetReference = null;
            obj.prop.value = null;
            allocate();
        }
        l += 1;
        return 10;
    };
    var obj = {
        x0 : {
            value : 0
        },
        x1 : {
            value : 0
        },
        x2 : {
            value : 0
        },
        x3 : {
            value : 0
        },
        x4 : {
            value : 0
        },
        x5 : {
            value : 0
        },
        x6 : {
            value : 0
        },
        x7 : {
            value : 0
        },
        x8 : {
            value : 0
        },
        x9 : {
            value : 0
        },
        x10 : {
            value : 0
        },
        length : {
            value : o
        },
        prop : {
            value : targetReference
        },
        beast : {
            value : 0
        }
    };
    var array2 = [];
    var expectedLength = targetReference.length
    Object.defineProperties(array2, obj);
    if (array2.prop.length != expectedLength)
        throw "fail";
}
test();
