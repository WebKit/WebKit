description('Tests that we find controls if they have a range limitation and are out-of-range.');

var parentDiv = document.createElement('div');
document.body.appendChild(parentDiv);
parentDiv.innerHTML = '<input id="number1" type="number" min=0 max=10 value=50><input id="date1" type="date" min="2010-12-28" max="2010-12-30" value="2011-01-01"><input id="text1" type="text" min=0 max=10 value=50><input id="checkbox1" type="checkbox"><input id="radio1" type="radio">';

shouldBe('document.querySelector("input[type=number]:out-of-range").id', '"number1"');
shouldBe('document.querySelector("input[type=date]:out-of-range").id', '"date1"');
shouldBe('document.querySelectorAll(":out-of-range").length', '2');

debug("");
debug("When the value becomes in-range dynamically, we do not find the control anymore");
document.getElementById("number1").value = 5;
document.getElementById("date1").value = "2010-12-28";

shouldBe('document.querySelector("input[type=number]:out-of-range")', 'null');
shouldBe('document.querySelector("input[type=date]:out-of-range")', 'null');
shouldBe('document.querySelectorAll(":out-of-range").length', '0');
