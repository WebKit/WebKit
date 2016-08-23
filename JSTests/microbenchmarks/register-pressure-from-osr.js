function foo(o, p, q, r, s, t, u) {
    var a = o.f;
    var b = p.f;
    var c = q.f;
    var d = r.f;
    var e = s.f;
    var f = t.f;
    var g = u.f;
    
    a++;
    b++;
    c++;
    d++;
    e++;
    f++;
    g++;
    
    var h = o.f;
    var i = p.f;
    var j = q.f;
    var k = r.f;
    var l = s.f;
    var m = t.f;
    var n = u.f;
    
    return a + b + c + d + e + f + g + h + i + j + k + l + m + n;
}

var o = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};
var p = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};
var q = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};
var r = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};
var s = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};
var t = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};
var u = {a:1, b:2, c:3, d:4, e:5, g:7, h:8, i:9, f:6};

var result = 0;
for (var i = 0; i < 1000000; ++i)
    result += foo(o, p, q, r, s, t, u);

if (result != 91000000)
    throw "Bad result: " + result;


