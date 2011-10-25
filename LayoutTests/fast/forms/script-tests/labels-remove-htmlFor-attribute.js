description('Test that the labels list of a form control is dynamically updated when the htmlFor attribute of a label is removed.');

var parent = document.createElement('div');

parent.innerHTML = '<div id="div1"></div><div id="div2"><label id="label1" for="id1"></label><label id="label2" for="id2"></label><label id="label3" for="id3"></label><label id="label4" for="id4"></label><button id="id1"></button><input id="id2"><select id="id3"></select><textarea id="id4"></textarea></div>';

document.body.appendChild(parent);

label = document.getElementById("label1");
labels = document.getElementById("id1").labels;
shouldBe('labels.length', '1');
label.removeAttribute("for");
shouldBe('labels.length', '0');

label = document.getElementById("label2");
labels = document.getElementById("id2").labels;
shouldBe('labels.length', '1');
label.removeAttribute("for");
shouldBe('labels.length', '0');

label = document.getElementById("label3");
labels = document.getElementById("id3").labels;
shouldBe('labels.length', '1');
label.removeAttribute("for");
shouldBe('labels.length', '0');

label = document.getElementById("label4");
labels = document.getElementById("id4").labels;
shouldBe('labels.length', '1');
label.removeAttribute("for");
shouldBe('labels.length', '0');

