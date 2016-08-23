var thing0 = Symbol();
var thing1 = Symbol();
var thing2 = Symbol();
var thing3 = Symbol();
var thing4 = Symbol();
var thing5 = Symbol();
var thing6 = Symbol();
var thing7 = Symbol();
var thing8 = Symbol();
var thing9 = Symbol();
var thing10 = Symbol();
var thing11 = Symbol();
var thing12 = Symbol();
var thing13 = Symbol();
var thing14 = Symbol();
var thing15 = Symbol();
var thing16 = Symbol();
var thing17 = Symbol();
var thing18 = Symbol();
var thing19 = Symbol();
var thing20 = Symbol();
var thing21 = Symbol();
var thing22 = Symbol();
var thing23 = Symbol();
var thing24 = Symbol();
var thing25 = Symbol();
var thing26 = Symbol();
var thing27 = Symbol();
var thing28 = Symbol();
var thing29 = Symbol();
var thing30 = Symbol();
var thing31 = Symbol();
var thing32 = Symbol();
var thing33 = Symbol();
var thing34 = Symbol();
var thing35 = Symbol();
var thing36 = Symbol();
var thing37 = Symbol();
var thing38 = Symbol();
var thing39 = Symbol();
var thing40 = Symbol();
var thing41 = Symbol();
var thing42 = Symbol();
var thing43 = Symbol();
var thing44 = Symbol();
var thing45 = Symbol();
var thing46 = Symbol();
var thing47 = Symbol();
var thing48 = Symbol();
var thing49 = Symbol();
var thing50 = Symbol();
var thing51 = Symbol();
var thing52 = Symbol();
var thing53 = Symbol();
var thing54 = Symbol();
var thing55 = Symbol();
var thing56 = Symbol();
var thing57 = Symbol();
var thing58 = Symbol();
var thing59 = Symbol();
var thing60 = Symbol();
var thing61 = Symbol();
var thing62 = Symbol();

var things = [];
for (var i = 0; i < 63; ++i)
    things.push(eval("thing" + i));

function foo(o) {
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        var value = things[i & 63];
        switch (value) {
        case thing0:
            result += o.a;
            break;
        case thing1:
            result += o.b;
            break;
        case thing2:
            result += o.c;
            break;
        case thing3:
            result += o.d;
            break;
        case thing4:
            result += o.e;
            break;
        case thing5:
            result += o.f;
            break;
        case thing6:
            result += o.g;
            break;
        case thing7:
            result += o.h;
            break;
        case thing8:
            result += o.i;
            break;
        case thing9:
            result += o.j;
            break;
        case thing10:
            result += o.k;
            break;
        case thing11:
            result += o.a;
            break;
        case thing12:
            result += o.b;
            break;
        case thing13:
            result += o.c;
            break;
        case thing14:
            result += o.d;
            break;
        case thing15:
            result += o.e;
            break;
        case thing16:
            result += o.f;
            break;
        case thing17:
            result += o.g;
            break;
        case thing18:
            result += o.h;
            break;
        case thing19:
            result += o.i;
            break;
        case thing20:
            result += o.j;
            break;
        case thing21:
            result += o.k;
            break;
        case thing22:
            result += o.k;
            break;
        case thing23:
            result += o.a;
            break;
        case thing24:
            result += o.b;
            break;
        case thing25:
            result += o.c;
            break;
        case thing26:
            result += o.d;
            break;
        case thing27:
            result += o.e;
            break;
        case thing28:
            result += o.f;
            break;
        case thing29:
            result += o.g;
            break;
        case thing30:
            result += o.h;
            break;
        case thing31:
            result += o.i;
            break;
        case thing32:
            result += o.j;
            break;
        case thing33:
            result += o.k;
            break;
        case thing34:
            result += o.k;
            break;
        case thing35:
            result += o.k;
            break;
        case thing36:
            result += o.a;
            break;
        case thing37:
            result += o.b;
            break;
        case thing38:
            result += o.c;
            break;
        case thing39:
            result += o.d;
            break;
        case thing40:
            result += o.e;
            break;
        case thing41:
            result += o.f;
            break;
        case thing42:
            result += o.g;
            break;
        case thing43:
            result += o.h;
            break;
        case thing44:
            result += o.i;
            break;
        case thing45:
            result += o.j;
            break;
        case thing46:
            result += o.k;
            break;
        case thing47:
            result += o.i;
            break;
        case thing48:
            result += o.j;
            break;
        case thing49:
            result += o.k;
            break;
        case thing50:
            result += o.k;
            break;
        case thing51:
            result += o.k;
            break;
        case thing52:
            result += o.a;
            break;
        case thing53:
            result += o.b;
            break;
        case thing54:
            result += o.c;
            break;
        case thing55:
            result += o.d;
            break;
        case thing56:
            result += o.e;
            break;
        case thing57:
            result += o.f;
            break;
        case thing58:
            result += o.g;
            break;
        case thing59:
            result += o.h;
            break;
        case thing60:
            result += o.i;
            break;
        case thing61:
            result += o.j;
            break;
        case thing62:
            result += o.k;
            break;
        default:
            result += o.z;
            break;
        }
    }
    return result;
}

(function() {
    var o = {a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10, k:11, z:100};
    var result = 0;
    for (var i = 0; i < 1000; ++i)
        result += foo(o);
    if (result != 7966000)
        throw "Error: bad result: " + result;
})();

