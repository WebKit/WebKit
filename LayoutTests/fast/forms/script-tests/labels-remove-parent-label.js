description('Test that the labels list of a form control is dynamically updated when removing a label as parent for that form control. ');

var parent = document.createElement('div');

parent.innerHTML = '<div id="div1"></div><div id="div2"><label><button id="id1"></button></label><label><input id="id2"></label><label><select id="id3"></select></label><label><textarea id="id4"></textarea></label></div>';


document.body.appendChild(parent);

labels = document.getElementById("id1").labels;
shouldBe('labels.length', '1');
document.getElementById("div1").appendChild(document.getElementById("id1"));
shouldBe('labels.length', '0');

labels = document.getElementById("id2").labels;
shouldBe('labels.length', '1');
document.getElementById("div1").appendChild(document.getElementById("id2"));
shouldBe('labels.length', '0');

labels = document.getElementById("id3").labels;
shouldBe('labels.length', '1');
document.getElementById("div1").appendChild(document.getElementById("id3"));
shouldBe('labels.length', '0');

labels = document.getElementById("id4").labels;
shouldBe('labels.length', '1');
document.getElementById("div1").appendChild(document.getElementById("id4"));
shouldBe('labels.length', '0');
