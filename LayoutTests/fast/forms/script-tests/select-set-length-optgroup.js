description('This test that setting HTMLSelectElement.length respects optgroups.');

var wrapper = document.createElement('div');
document.body.appendChild(wrapper);
wrapper.innerHTML = '<select id="theSelect">'+
                    '<optgroup label="foo" id="theOptGroup">'+
                    '<option id="optionInGroup"></option>'+
                    '</optgroup>'+
                    '</select>';

var sel = document.getElementById('theSelect');
shouldBe('sel.length', '1');

var og = document.getElementById('theOptGroup');

sel.length = 2;
shouldBe('sel.length', '2');
shouldBe('og.childElementCount', '1');

sel.length = 1;
shouldBe('sel.length', '1');
shouldBe('og.childElementCount', '1');

sel.insertBefore(document.createElement('option'), og);

sel.length = 1;
shouldBe('sel.length', '1');
shouldBe('og.childElementCount', '0');
