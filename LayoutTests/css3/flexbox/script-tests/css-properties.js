description('Tests being able to set the display to -webkit-flexbox and -webkit-inline-flexbox.');

var flexbox = document.getElementById("flexbox");

flexbox.style.display = '-webkit-flexbox';
shouldBeEqualToString('flexbox.style.display', '-webkit-flexbox');

flexbox.style.display = 'inline';
shouldBeEqualToString('flexbox.style.display', 'inline');

flexbox.style.display = '-webkit-inline-flexbox';
shouldBeEqualToString('flexbox.style.display', '-webkit-inline-flexbox');

flexbox.style.display = 'junk';
shouldBeEqualToString('flexbox.style.display', '-webkit-inline-flexbox');

flexbox.style.display = 'block';
shouldBeEqualToString('flexbox.style.display', 'block');


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


shouldBeEqualToString('flexbox.style.webkitFlexPack', '');
// The initial value is 'start'.
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexPack', 'start');

flexbox.style.webkitFlexPack = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexPack', '');

flexbox.style.webkitFlexPack = 'start';
shouldBeEqualToString('flexbox.style.webkitFlexPack', 'start');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexPack', 'start');

flexbox.style.webkitFlexPack = 'end';
shouldBeEqualToString('flexbox.style.webkitFlexPack', 'end');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexPack', 'end');

flexbox.style.webkitFlexPack = 'center';
shouldBeEqualToString('flexbox.style.webkitFlexPack', 'center');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexPack', 'center');

flexbox.style.webkitFlexPack = 'justify';
shouldBeEqualToString('flexbox.style.webkitFlexPack', 'justify');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexPack', 'justify');

flexbox.style.webkitFlexPack = '';
shouldBeEqualToString('flexbox.style.webkitFlexPack', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexPack', 'start');


shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');
// The initial value is 'stretch'.
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');

flexbox.style.webkitFlexAlign = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');

flexbox.style.webkitFlexAlign = 'before';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'before');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'before');

flexbox.style.webkitFlexAlign = 'after';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'after');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'after');

flexbox.style.webkitFlexAlign = 'middle';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'middle');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'middle');

flexbox.style.webkitFlexAlign = 'stretch';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');

flexbox.style.webkitFlexAlign = 'baseline';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'baseline');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'baseline');

flexbox.style.webkitFlexAlign = '';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');

shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row');

flexbox.style.webkitFlexFlow = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');

flexbox.style.webkitFlexFlow = 'row';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'row');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row');

flexbox.style.webkitFlexFlow = 'row-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'row-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row-reverse');

flexbox.style.webkitFlexFlow = 'column';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'column');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'column');

flexbox.style.webkitFlexFlow = 'column-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'column-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'column-reverse');

flexbox.style.webkitFlexFlow = 'horizontal';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'horizontal');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'horizontal');

flexbox.style.webkitFlexFlow = 'horizontal-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'horizontal-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'horizontal-reverse');

flexbox.style.webkitFlexFlow = 'horizontal-ltr';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'horizontal-ltr');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'horizontal-ltr');

flexbox.style.webkitFlexFlow = 'horizontal-rtl';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'horizontal-rtl');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'horizontal-rtl');

flexbox.style.webkitFlexFlow = 'vertical';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'vertical');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'vertical');

flexbox.style.webkitFlexFlow = 'vertical-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'vertical-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'vertical-reverse');

flexbox.style.webkitFlexFlow = 'vertical-ttb';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'vertical-ttb');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'vertical-ttb');

flexbox.style.webkitFlexFlow = 'vertical-btt';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'vertical-btt');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'vertical-btt');

successfullyParsed = true;
