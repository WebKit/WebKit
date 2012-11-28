description('Tests that we find controls if they have a range limitation and are in-range.');

var parentDiv = document.createElement('div');
document.body.appendChild(parentDiv);
parentDiv.innerHTML = '<input id="number1" type="number" min=0 max=10 value=5><input id="range1" type="range" min=0 max=10 value=5><input id="text1" type="text" min=0 max=10 value=5><input id="checkbox1" type="checkbox">    <input id="radio1" type="radio">';

shouldBe('document.querySelector("input[type=number]:in-range").id', '"number1"');
shouldBe('document.querySelector("input[type=range]:in-range").id', '"range1"');
shouldBe('document.querySelectorAll(":in-range").length', '2');
