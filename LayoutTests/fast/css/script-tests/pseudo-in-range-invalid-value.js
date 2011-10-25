description('Tests that invalid values are considered in-range.');

var parentDiv = document.createElement('div');
document.body.appendChild(parentDiv);
parentDiv.innerHTML = '<input id="number1" type="number" min=0 max=10 value="ABC"><input id="range1" type="range" min=0 max=10 value="ABC"><input id="date1" type="date" min="2010-12-28" max="2010-12-28" value="ABC"><input id="text1" type="text" min=0 max=10 value="ABC"><input id="checkbox1" type="checkbox">    <input id="radio1" type="radio">';

shouldBe('document.querySelector("input[type=number]:in-range").id', '"number1"');
shouldBe('document.querySelector("input[type=range]:in-range").id', '"range1"');
shouldBe('document.querySelector("input[type=date]:in-range").id', '"date1"');
shouldBe('document.querySelectorAll(":in-range").length', '3');
