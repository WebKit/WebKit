description('Test that the labels list of a form control is dynamically updated when removing the htmlFor attribute that pointed to that form control.');

var parent = document.createElement('div');

parent.innerHTML = '<div id="div1"></div><div id="div2"><button id="id1"></button><input id="id2"><select id="id3"></select><textarea id="id4"></textarea></div><label id="l1" for="id1"></label><label id="l2" for="id2"></label><label id="l3" for="id3"></label><label id="l4" for="id4"></label>';

document.body.appendChild(parent);

labels = document.getElementById("id1").labels;
shouldBe('labels.length', '1');
document.getElementById("l1").htmlFor = "";
shouldBe('labels.length', '0');

labels = document.getElementById("id2").labels;
shouldBe('labels.length', '1');
document.getElementById("l2").htmlFor = "";
shouldBe('labels.length', '0');

labels = document.getElementById("id3").labels;
shouldBe('labels.length', '1');
document.getElementById("l3").htmlFor = "";
shouldBe('labels.length', '0');

labels = document.getElementById("id4").labels;
shouldBe('labels.length', '1');
document.getElementById("l4").htmlFor = "";
shouldBe('labels.length', '0');
