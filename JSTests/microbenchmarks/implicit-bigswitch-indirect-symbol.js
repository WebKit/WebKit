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
var thing63 = Symbol();

var things = [];
for (var i = 0; i < 64; ++i)
    things.push(eval("thing" + i));

function foo(o) {
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        var value = things[i & 63];
        if (value === thing0)
            result += o.a;
        else if (value === thing1)
            result += o.b;
        else if (value === thing2)
            result += o.c;
        else if (value === thing3)
            result += o.d;
        else if (value === thing4)
            result += o.e;
        else if (value === thing5)
            result += o.f;
        else if (value === thing6)
            result += o.g;
        else if (value === thing7)
            result += o.h;
        else if (value === thing8)
            result += o.i;
        else if (value === thing9)
            result += o.j;
        else if (value === thing10)
            result += o.k;
        else if (value === thing11)
            result += o.a;
        else if (value === thing12)
            result += o.b;
        else if (value === thing13)
            result += o.c;
        else if (value === thing14)
            result += o.d;
        else if (value === thing15)
            result += o.e;
        else if (value === thing16)
            result += o.f;
        else if (value === thing17)
            result += o.g;
        else if (value === thing18)
            result += o.h;
        else if (value === thing19)
            result += o.i;
        else if (value === thing20)
            result += o.j;
        else if (value === thing21)
            result += o.k;
        else if (value === thing22)
            result += o.k;
        else if (value === thing23)
            result += o.a;
        else if (value === thing24)
            result += o.b;
        else if (value === thing25)
            result += o.c;
        else if (value === thing26)
            result += o.d;
        else if (value === thing27)
            result += o.e;
        else if (value === thing28)
            result += o.f;
        else if (value === thing29)
            result += o.g;
        else if (value === thing30)
            result += o.h;
        else if (value === thing31)
            result += o.i;
        else if (value === thing32)
            result += o.j;
        else if (value === thing33)
            result += o.k;
        else if (value === thing34)
            result += o.k;
        else if (value === thing35)
            result += o.k;
        else if (value === thing36)
            result += o.a;
        else if (value === thing37)
            result += o.b;
        else if (value === thing38)
            result += o.c;
        else if (value === thing39)
            result += o.d;
        else if (value === thing40)
            result += o.e;
        else if (value === thing41)
            result += o.f;
        else if (value === thing42)
            result += o.g;
        else if (value === thing43)
            result += o.h;
        else if (value === thing44)
            result += o.i;
        else if (value === thing45)
            result += o.j;
        else if (value === thing46)
            result += o.k;
        else if (value === thing47)
            result += o.i;
        else if (value === thing48)
            result += o.j;
        else if (value === thing49)
            result += o.k;
        else if (value === thing50)
            result += o.k;
        else if (value === thing51)
            result += o.k;
        else if (value === thing52)
            result += o.a;
        else if (value === thing53)
            result += o.b;
        else if (value === thing54)
            result += o.c;
        else if (value === thing55)
            result += o.d;
        else if (value === thing56)
            result += o.e;
        else if (value === thing57)
            result += o.f;
        else if (value === thing58)
            result += o.g;
        else if (value === thing59)
            result += o.h;
        else if (value === thing60)
            result += o.i;
        else if (value === thing61)
            result += o.j;
        else if (value === thing62)
            result += o.k;
        else
            result += o.z;
    }
    return result;
}

(function() {
    var o = {a:1, b:2, c:3, d:4, e:5, f:6, g:7, h:8, i:9, j:10, k:11, z:100};
    var result = 0;
    for (var i = 0; i < 4000; ++i)
        result += foo(o);
    if (result != 31864000)
        throw "Error: bad result: " + result;
})();

