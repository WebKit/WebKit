description('Various tests for the the hidden IDL attribute.');

var testParent = document.createElement('div');
document.body.appendChild(testParent);

debug('The IDL attribute reflects what is present in markup:');
testParent.innerHTML = '<div id=h1 hidden></div><div id=h2 hidden=false></div><div id=h3 hidden=off></div><div id=s1></div>';
var h1 = document.getElementById("h1");
var h2 = document.getElementById("h2");
var h3 = document.getElementById("h3");
var s1 = document.getElementById("s1");

shouldBeTrue('h1.hidden');
shouldBeTrue('h2.hidden');
shouldBeTrue('h3.hidden');
shouldBeFalse('s1.hidden');

debug('Changes via DOM Core are reflected through the IDL attribute:');

shouldBeFalse('(h1.removeAttribute("hidden"), h1.hidden)');
shouldBeTrue('(h1.setAttribute("hidden", ""), h1.hidden)');
shouldBeTrue('(h2.setAttribute("hidden", ""), h2.hidden)');
shouldBeTrue('(s1.setAttribute("hidden", ""), s1.hidden)');

debug('Changes via IDL attribute are reflected in the core DOM:');

shouldBe('(h3.hidden = false, h3.getAttribute("hidden"))', 'null');
shouldBe('(h3.hidden = true, h3.getAttribute("hidden"))', '""');
