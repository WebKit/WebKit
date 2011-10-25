description('Test that the labels list of a form control is dynamically updated when the htmlFor attribute of a label changes to point to that control. ');

var parent = document.createElement('div');

parent.innerHTML = '<div id="div1"></div><div id="div2"><label id="labelId"></label><button id="id1"></button><input id="id2"><select id="id3"></select><textarea id="id4"></textarea></div>';

document.body.appendChild(parent);

label = document.getElementById("labelId");
labels = document.getElementById("id1").labels;
shouldBe('labels.length', '0');
label.setAttribute("for", "id1");
shouldBe('labels.length', '1');

label = document.getElementById("labelId");
labels = document.getElementById("id2").labels;
shouldBe('labels.length', '0');
label.setAttribute("for", "id2");
shouldBe('labels.length', '1');

label = document.getElementById("labelId");
labels = document.getElementById("id3").labels;
shouldBe('labels.length', '0');
label.setAttribute("for", "id3");
shouldBe('labels.length', '1');

label = document.getElementById("labelId");
labels = document.getElementById("id4").labels;
shouldBe('labels.length', '0');
label.setAttribute("for", "id4");
shouldBe('labels.length', '1');

