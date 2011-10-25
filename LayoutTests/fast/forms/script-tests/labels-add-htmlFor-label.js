description('Test that the labels list of a form control is dynamically updated when adding a label whose htmlFor points to that control. ');

var parent = document.createElement('div');

parent.innerHTML = '<div id="div1"></div><div id="div2"><button id="id1"><button><input id="id2"><select id="id3"></select><textarea id="id4"></textarea></div>';

document.body.appendChild(parent);

labels = document.getElementById("id1").labels;
shouldBe('labels.length', '0');
label = document.createElement("label");
label.htmlFor = "id1";
document.getElementById("div1").appendChild(label);
shouldBe('labels.length', '1');

labels = document.getElementById("id2").labels;
shouldBe('labels.length', '0');
label = document.createElement("label");
label.htmlFor = "id2";
document.getElementById("div1").appendChild(label);
shouldBe('labels.length', '1');

labels = document.getElementById("id3").labels;
shouldBe('labels.length', '0');
label = document.createElement("label");
label.htmlFor = "id3";
document.getElementById("div1").appendChild(label);
shouldBe('labels.length', '1');

labels = document.getElementById("id4").labels;
shouldBe('labels.length', '0');
label = document.createElement("label");
label.htmlFor = "id4";
document.getElementById("div1").appendChild(label);
shouldBe('labels.length', '1');
