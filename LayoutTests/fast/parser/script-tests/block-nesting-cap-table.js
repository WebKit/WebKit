description('Test that the HTML parser does not allow the nesting depth of "block-level" elements to exceed 4096 when using nested table tag');

var depth = 1028;  // <table><tbody><tr><td> consumes 4 blocks. (1028 * 4 = 4112 > 4096).
var markup = "";
var i;
for (i = 0; i < depth; ++i)
    markup += "<table id='t" + i + "'><tbody><tr><td id='td" + i + "'>";
var doc = document.implementation.createHTMLDocument();
doc.body.innerHTML = markup;

var t1023 = doc.getElementById("t1023");
var t1024 = doc.getElementById("t1024");

var td1022 = doc.getElementById("td1022");

shouldBe("t1024.parentNode === td1022", "true");
shouldBe("t1023.parentNode === td1022", "true");
shouldBe("t1024.previousSibling === t1023", "true");

var successfullyParsed = true;
