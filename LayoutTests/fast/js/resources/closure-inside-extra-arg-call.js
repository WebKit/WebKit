description(

"This test checks that activation objects for functions called with too many arguments are created properly."

);


var c1;

function f1()
{
    var a = "x";
    var b = "y";
    var c = a + b;
    var d = a + b + c;

    c1 = function() { return d; }
}

f1(0, 0, 0, 0, 0, 0, 0, 0, 0);

function s1() {
    shouldBe("c1()", '"xyxy"');
}

function t1() {
    var a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
    s1();
}

t1();

var c2;

function f2()
{
    var a = "x";
    var b = "y";
    var c = a + b;
    var d = a + b + c;

    c2 = function() { return d; }
}

new f2(0, 0, 0, 0, 0, 0, 0, 0, 0);

function s2() {
    shouldBe("c2()", '"xyxy"');
}

function t2() {
    var a, b, c, d, e, f, g, h, i, j, k, l, m, n, o, p;
    s2();
}

t2();

successfullyParsed = true;
