function foo(o) {
    var result = 0;
    for (var i = 0; i < 1000; ++i) {
        var value = i & 63;
        switch (value) {
        case 0:
            result += o.a;
            break;
        case 1:
            result += o.b;
            break;
        case 2:
            result += o.c;
            break;
        case 3:
            result += o.d;
            break;
        case 4:
            result += o.e;
            break;
        case 5:
            result += o.f;
            break;
        case 6:
            result += o.g;
            break;
        case 7:
            result += o.h;
            break;
        case 8:
            result += o.i;
            break;
        case 9:
            result += o.j;
            break;
        case 10:
            result += o.k;
            break;
        case 11:
            result += o.a;
            break;
        case 12:
            result += o.b;
            break;
        case 13:
            result += o.c;
            break;
        case 14:
            result += o.d;
            break;
        case 15:
            result += o.e;
            break;
        case 16:
            result += o.f;
            break;
        case 17:
            result += o.g;
            break;
        case 18:
            result += o.h;
            break;
        case 19:
            result += o.i;
            break;
        case 20:
            result += o.j;
            break;
        case 21:
            result += o.k;
            break;
        case 22:
            result += o.k;
            break;
        case 23:
            result += o.a;
            break;
        case 24:
            result += o.b;
            break;
        case 25:
            result += o.c;
            break;
        case 26:
            result += o.d;
            break;
        case 27:
            result += o.e;
            break;
        case 28:
            result += o.f;
            break;
        case 29:
            result += o.g;
            break;
        case 30:
            result += o.h;
            break;
        case 31:
            result += o.i;
            break;
        case 32:
            result += o.j;
            break;
        case 33:
            result += o.k;
            break;
        case 34:
            result += o.k;
            break;
        case 35:
            result += o.k;
            break;
        case 36:
            result += o.a;
            break;
        case 37:
            result += o.b;
            break;
        case 38:
            result += o.c;
            break;
        case 39:
            result += o.d;
            break;
        case 40:
            result += o.e;
            break;
        case 41:
            result += o.f;
            break;
        case 42:
            result += o.g;
            break;
        case 43:
            result += o.h;
            break;
        case 44:
            result += o.i;
            break;
        case 45:
            result += o.j;
            break;
        case 46:
            result += o.k;
            break;
        case 47:
            result += o.i;
            break;
        case 48:
            result += o.j;
            break;
        case 49:
            result += o.k;
            break;
        case 50:
            result += o.k;
            break;
        case 51:
            result += o.k;
            break;
        case 52:
            result += o.a;
            break;
        case 53:
            result += o.b;
            break;
        case 54:
            result += o.c;
            break;
        case 55:
            result += o.d;
            break;
        case 56:
            result += o.e;
            break;
        case 57:
            result += o.f;
            break;
        case 58:
            result += o.g;
            break;
        case 59:
            result += o.h;
            break;
        case 60:
            result += o.i;
            break;
        case 61:
            result += o.j;
            break;
        case 62:
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
    for (var i = 0; i < 10000; ++i)
        result += foo(o);
    if (result != 79660000)
        throw "Error: bad result: " + result;
})();

