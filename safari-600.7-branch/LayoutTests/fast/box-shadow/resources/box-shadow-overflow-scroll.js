description(
"Check if the size of box-shadow's blur and spread are considered as overflow size. See this bug for detail: https://bugs.webkit.org/show_bug.cgi?id=28017."
);

var element = null;

element = document.getElementById("horizontal");
element.scrollLeft = 500;
shouldBe("element.scrollLeft", "40+50+60");

element = document.getElementById("vertical");
element.scrollTop = 500;
shouldBe("element.scrollTop", "40+50+60");

element = document.getElementById("horizontal_flex");
element.scrollLeft = 500;
shouldBe("element.scrollLeft", "40+50+60");

element = document.getElementById("vertical_flex");
element.scrollTop = 500;
shouldBe("element.scrollTop", "40+50+60");

element = document.getElementById("horizontal_table");
element.scrollLeft = 500;
shouldBe("element.scrollLeft", "40+50+60");

element = document.getElementById("vertical_table");
element.scrollTop = 500;
shouldBe("element.scrollTop", "40+50+60");

// If we are in DTR, we don't need meaningless trailing messages.
if (window.testRunner) {
    document.getElementById("main").innerHTML = "";
}
