description("Test that the HTML parser correctly parses the comment-like string in the textarea.");

var element = document.createElement("div");
element.innerHTML = "<textarea><!-- </textarea> --> This should be part of the textarea</textarea>";
document.body.appendChild(element);

var textAreas = document.getElementsByTagName("textarea");
shouldBe("textAreas.length", "1");
shouldBeEqualToString("textAreas[0].innerHTML", "&lt;!-- &lt;/textarea&gt; --&gt; This should be part of the textarea");
