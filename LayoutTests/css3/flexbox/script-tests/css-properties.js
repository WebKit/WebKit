description('Tests being able to set the display to -webkit-flexbox and -webkit-inline-flexbox.');

var div = document.getElementById("flexbox");

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

var flexitem = document.getElementById("flexitem");
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '');

flexitem.style.webkitFlexOrder = 2;
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '2');

flexitem.style.webkitFlexOrder = -1;
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '-1');

flexitem.style.webkitFlexOrder = 0;
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '0');

// -webkit-flex-order must be an integer.
flexitem.style.webkitFlexOrder = 1.5;
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '0');

flexitem.style.webkitFlexOrder = "test";
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '0');

flexitem.style.webkitFlexOrder = '';
shouldBeEqualToString('flexitem.style.webkitFlexOrder', '');

successfullyParsed = true;
