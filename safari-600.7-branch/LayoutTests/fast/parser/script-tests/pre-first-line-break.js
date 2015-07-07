description('Test for handling of line breaks following the pre element.');

var element = document.createElement("div");

function roundTrip(string)
{
    element.innerHTML = string;
    return element.innerHTML;
}

shouldBe("roundTrip('<pre></pre>')", "'<pre></pre>'");
shouldBe("roundTrip('<pre>\\n</pre>')", "'<pre></pre>'");
shouldBe("roundTrip('<pre>\\n\\n</pre>')", "'<pre>\\n</pre>'");
shouldBe("roundTrip('<pre>x\\n</pre>')", "'<pre>x\\n</pre>'");
shouldBe("roundTrip('<pre>&lt;\\n</pre>')", "'<pre>&lt;\\n</pre>'");
shouldBe("roundTrip('<pre>&#61;\\n</pre>')", "'<pre>=\\n</pre>'");
shouldBe("roundTrip('<pre><a></a></pre>')", "'<pre><a></a></pre>'");
shouldBe("roundTrip('<pre><a>\\n</a></pre>')", "'<pre><a>\\n</a></pre>'");
shouldBe("roundTrip('<pre>\\n<a></a></pre>')", "'<pre><a></a></pre>'");
shouldBe("roundTrip('<pre>\\n<a>\\n</a></pre>')", "'<pre><a>\\n</a></pre>'");

shouldBe("roundTrip('<listing></listing>')", "'<listing></listing>'");
shouldBe("roundTrip('<listing>\\n</listing>')", "'<listing></listing>'");
shouldBe("roundTrip('<listing>\\n\\n</listing>')", "'<listing>\\n</listing>'");
shouldBe("roundTrip('<listing>x\\n</listing>')", "'<listing>x\\n</listing>'");
shouldBe("roundTrip('<listing>&lt;\\n</listing>')", "'<listing>&lt;\\n</listing>'");
shouldBe("roundTrip('<listing>&#61;\\n</listing>')", "'<listing>=\\n</listing>'");
shouldBe("roundTrip('<listing><a></a></listing>')", "'<listing><a></a></listing>'");
shouldBe("roundTrip('<listing><a>\\n</a></listing>')", "'<listing><a>\\n</a></listing>'");
shouldBe("roundTrip('<listing>\\n<a></a></listing>')", "'<listing><a></a></listing>'");
shouldBe("roundTrip('<listing>\\n<a>\\n</a></listing>')", "'<listing><a>\\n</a></listing>'");

shouldBe("roundTrip('<div></div>')", "'<div></div>'");
shouldBe("roundTrip('<div>\\n</div>')", "'<div>\\n</div>'");
shouldBe("roundTrip('<div>\\n\\n</div>')", "'<div>\\n\\n</div>'");
shouldBe("roundTrip('<div>x\\n</div>')", "'<div>x\\n</div>'");
shouldBe("roundTrip('<div>&lt;\\n</div>')", "'<div>&lt;\\n</div>'");
shouldBe("roundTrip('<div>&#61;\\n</div>')", "'<div>=\\n</div>'");
shouldBe("roundTrip('<div><a></a></div>')", "'<div><a></a></div>'");
shouldBe("roundTrip('<div><a>\\n</a></div>')", "'<div><a>\\n</a></div>'");
shouldBe("roundTrip('<div>\\n<a></a></div>')", "'<div>\\n<a></a></div>'");
shouldBe("roundTrip('<div>\\n<a>\\n</a></div>')", "'<div>\\n<a>\\n</a></div>'");
