description(
"This test checks hexadecimal color parsing."
);

var red = "rgb(255, 0, 0)";
var black = "rgb(0, 0, 0)";

for (var i=1;i<=5;i++)
{
    var div = document.getElementById("valid"+i);
    shouldBe("getComputedStyle(div, null).color", "red");
}

for (var i=1;i<=5;i++)
{
    var div = document.getElementById("invalid"+i);
    shouldBe("getComputedStyle(div, null).color", "black");
}
