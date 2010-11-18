var s = document.createElement('span');
s.style.fontSize = 0;
s.innerHTML = 'Text';
document.body.appendChild(s);

shouldBe("s.getBoundingClientRect().height", "0");

document.body.removeChild(s);

var successfullyParsed = true;
