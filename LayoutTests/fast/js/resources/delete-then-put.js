description(
'This tests for a problem with put after delete that existed at one point in the past.'
);

function props(o)
{
    var s = "";
    for (p in o) {
        if (s.length != 0)
            s += ",";
        s += p;
    }
    return s;
}

var a = { a:1, b:2, c:3, d:4, e:5 }

shouldBe("props(a)", "'a,b,c,d,e'");
debug("delete a.c");
delete a.c;
shouldBe("props(a)", "'a,b,d,e'");
debug("define getter named c");
a.__defineGetter__("c", function() { return 3 });
shouldBe("props(a)", "'a,b,d,e,c'");
debug("");

var successfullyParsed = true;
