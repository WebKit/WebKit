description("Check if changing flexbox's content doesn't change the state of scroll. See this bug for detail: https://bugs.webkit.org/show_bug.cgi?id=15135");

var element = null;

element = document.getElementById("vertical");
element.scrollTop = 108;
element.lastChild.data='meroL ipsum';
shouldBe("element.scrollTop", "108");

element = document.getElementById("horizontal");
element.scrollLeft = 108;
element.lastChild.data='fooooooooooooooo meroL ipsum';
shouldBe("element.scrollLeft", "108");

element = document.getElementById("nest");
element.scrollTop = 108;
element.lastChild.data='meroL ipsum';
shouldBe("element.scrollTop", "108");

element = document.getElementById("nest2");
element.scrollTop = 108;
element.lastChild.data='meroL ipsum';
shouldBe("element.scrollTop", "108");
