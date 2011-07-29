description('Tests being able to set the display to -webkit-flexbox and -webkit-inline-flexbox.');

var div = document.createElement('div');

div.style.display = '-webkit-flexbox';
shouldBeEqualToString('div.style.display', '-webkit-flexbox');

div.style.display = 'inline';
shouldBeEqualToString('div.style.display', 'inline');

div.style.display = '-webkit-inline-flexbox';
shouldBeEqualToString('div.style.display', '-webkit-inline-flexbox');

div.style.display = 'junk';
shouldBeEqualToString('div.style.display', '-webkit-inline-flexbox');

div.style.display = 'block';
shouldBeEqualToString('div.style.display', 'block');

successfullyParsed = true;
