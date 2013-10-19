description(
"This test checks the behavior of the spread construct."
);

function f(a,b,c,d)
{
    args = arguments;
    passedThis = this;
    shouldBe("passedThis", "o")
    shouldBe("args[0]", "1")
    shouldBe("args[1]", "undefined")
    shouldBe("args[2]", "null")
    shouldBe("args[3]", "4")
}

var o = {}
o.f = f;
var test1 = [1, undefined, null, 4]
var test2 = [1, , null, 4]
var test3 = {length: 4, 0: 1, 2: null, 3: 4}
var test4 = {length: 4, 0: 1, 1: undefined, 2: null, 3: 4}
o.f(...test1)
o.f(...test2)
o.f(...test3)
o.f(...test4)

var h=eval('"f"')
o[h](...test1)
o[h](...test2)
o[h](...test3)
o[h](...test4)

function g()
{
    o.f(...arguments)
}

g.apply(null, test1)
g.apply(null, test2)
g.apply(null, test3)
g.apply(null, test4)

g(...test1)
g(...test2)
g(...test3)
g(...test4)

var a=[1,2,3]

shouldBe("a", "[1,2,3]")
shouldBe("[...a]", "[1,2,3]")
a=[...a]
shouldBe("[...a]", "[1,2,3]")
shouldBe("[...a,...[...a]]", "[1,2,3,1,2,3]")
shouldBe("[,,,...a]", "[,,,1,2,3]")
shouldBe("[...a,,,].join('|')", "[1,2,3,,,].join('|')")
shouldBe("[,...a,4]", "[,1,2,3,4]")
shouldBe("[,...a,,5]", "[,1,2,3,,5]")
shouldBe("[...a.keys()]", "[0,1,2]")
shouldBe("[...a.entries()].join('|')", "[[0,1],[1,2],[2,3]].join('|')")
Array.prototype.__defineSetter__(0, function(){ fail() });
Array.prototype.__defineSetter__(1, function(){ fail() });
Array.prototype.__defineSetter__(2, function(){ fail() });
shouldBe("[...a]", "[1,2,3]")



