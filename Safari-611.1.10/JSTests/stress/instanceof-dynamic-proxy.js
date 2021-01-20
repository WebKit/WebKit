class Foo0 { }
class Foo1 { }
class Foo2 { }
class Foo3 { }
class Foo4 { }
class Foo5 { }
class Foo6 { }
class Foo7 { }
class Foo8 { }
class Foo9 { }
class Foo10 { }
class Foo11 { }
class Foo12 { }
class Foo13 { }
class Foo14 { }
class Foo15 { }
class Foo16 { }
class Foo17 { }
class Foo18 { }
class Foo19 { }
class Foo20 { }
class Foo21 { }
class Foo22 { }
class Foo23 { }
class Foo24 { }
class Foo25 { }
class Foo26 { }
class Foo27 { }
class Foo28 { }
class Foo29 { }
class Foo30 { }
class Foo31 { }
class Foo32 { }
class Foo33 { }
class Foo34 { }
class Foo35 { }
class Foo36 { }
class Foo37 { }
class Foo38 { }
class Foo39 { }
class Foo40 { }
class Foo41 { }
class Foo42 { }
class Foo43 { }
class Foo44 { }
class Foo45 { }
class Foo46 { }
class Foo47 { }
class Foo48 { }
class Foo49 { }
class Foo50 { }
class Foo51 { }
class Foo52 { }
class Foo53 { }
class Foo54 { }
class Foo55 { }
class Foo56 { }
class Foo57 { }
class Foo58 { }
class Foo59 { }
class Foo60 { }
class Foo61 { }
class Foo62 { }
class Foo63 { }
class Foo64 { }
class Foo65 { }
class Foo66 { }
class Foo67 { }
class Foo68 { }
class Foo69 { }
class Foo70 { }
class Foo71 { }
class Foo72 { }
class Foo73 { }
class Foo74 { }
class Foo75 { }
class Foo76 { }
class Foo77 { }
class Foo78 { }
class Foo79 { }
class Foo80 { }
class Foo81 { }
class Foo82 { }
class Foo83 { }
class Foo84 { }
class Foo85 { }
class Foo86 { }
class Foo87 { }
class Foo88 { }
class Foo89 { }
class Foo90 { }
class Foo91 { }
class Foo92 { }
class Foo93 { }
class Foo94 { }
class Foo95 { }
class Foo96 { }
class Foo97 { }
class Foo98 { }
class Foo99 { }

var foos = [new Foo0(), new Foo1(), new Foo2(), new Foo3(), new Foo4(), new Foo5(), new Foo6(), new Foo7(), new Foo8(), new Foo9(), new Foo10(), new Foo11(), new Foo12(), new Foo13(), new Foo14(), new Foo15(), new Foo16(), new Foo17(), new Foo18(), new Foo19(), new Foo20(), new Foo21(), new Foo22(), new Foo23(), new Foo24(), new Foo25(), new Foo26(), new Foo27(), new Foo28(), new Foo29(), new Foo30(), new Foo31(), new Foo32(), new Foo33(), new Foo34(), new Foo35(), new Foo36(), new Foo37(), new Foo38(), new Foo39(), new Foo40(), new Foo41(), new Foo42(), new Foo43(), new Foo44(), new Foo45(), new Foo46(), new Foo47(), new Foo48(), new Foo49(), new Foo50(), new Foo51(), new Foo52(), new Foo53(), new Foo54(), new Foo55(), new Foo56(), new Foo57(), new Foo58(), new Foo59(), new Foo60(), new Foo61(), new Foo62(), new Foo63(), new Foo64(), new Foo65(), new Foo66(), new Foo67(), new Foo68(), new Foo69(), new Foo70(), new Foo71(), new Foo72(), new Foo73(), new Foo74(), new Foo75(), new Foo76(), new Foo77(), new Foo78(), new Foo79(), new Foo80(), new Foo81(), new Foo82(), new Foo83(), new Foo84(), new Foo85(), new Foo86(), new Foo87(), new Foo88(), new Foo89(), new Foo90(), new Foo91(), new Foo92(), new Foo93(), new Foo94(), new Foo95(), new Foo96(), new Foo97(), new Foo98(), new Foo99()];

class Foo { }

function Bar() { }

var numberOfGetPrototypeOfCalls = 0;

var doBadThings = function() { };

Bar.prototype = new Proxy(
    {},
    {
        getPrototypeOf()
        {
            numberOfGetPrototypeOfCalls++;
            doBadThings();
            return Foo.prototype;
        }
    });

var o = {f:42};

function foo(o, p)
{
    var result = o.f;
    var _ = p instanceof Foo;
    return result + o.f;
}

noInline(foo);

for (var i = 0; i < 10000; ++i) {
    var result = foo({f:42}, foos[i % foos.length]);
    if (result != 84)
        throw "Error: bad result in loop: " + result;
}

var globalO = {f:42};
var didCallGetter = false;
doBadThings = function() {
    globalO.f = 43;
};

var result = foo(globalO, new Bar());
if (result != 85)
    throw "Error: bad result at end: " + result;
if (numberOfGetPrototypeOfCalls != 1)
    throw "Error: did not call getPrototypeOf() the right number of times at end";
