//@ skip if $model == "Apple Watch Series 3" # added by mark-jsc-stress-test.py
//@ $skipModes << :lockdown if $buildType == "debug"

function foo(o)
{
    var result = 0;
    for (var i = 0; i < 100; ++i)
        result += o.f.g.h.i.j.k.l.m.n.o.p.q.r.s.t.u.v.w.x.y.z;
    return result;
}

noInline(foo);

for (var i = 0; i < 1000; ++i) {
    var o = {f:{g:{h:{i:{j:{k:{l:{m:{n:{o:{p:{q:{r:{s:{t:{u:{v:{w:{x:{y:{z:42}}}}}}}}}}}}}}}}}}}}};
    for (var j = 0; j < 100; ++j) {
        var result = foo(o);
        if (result != 4200)
            throw "Bad result in loop: " + result;
    }
}

