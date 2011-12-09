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

flexbox.style.webkitFlexAlign = 'start';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'start');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'start');

flexbox.style.webkitFlexAlign = 'end';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'end');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'end');

flexbox.style.webkitFlexAlign = 'center';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'center');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'center');

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
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row nowrap');

flexbox.style.webkitFlexFlow = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');

var flows = ['', 'row', 'row-reverse', 'column', 'column-reverse'];
var wraps = ['', 'nowrap', 'wrap', 'wrap-reverse'];
flows.forEach(function(flow) {
    wraps.forEach(function(wrap) {
        flexbox.style.webkitFlexFlow = flow + ' ' + wrap;
        shouldBeEqualToString('flexbox.style.webkitFlexFlow', (flow + ' ' + wrap).replace(/^ /, '').replace(/ $/, ''));
        if (!flow)
            flow = 'row';
        if (!wrap)
            wrap = 'nowrap';
        shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', flow + ' ' + wrap);
    });
});

flexbox.style.webkitFlexFlow = '';
flexbox.style.webkitFlexFlow = 'wrap wrap-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
flexbox.style.webkitFlexFlow = 'wrap-reverse column';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
flexbox.style.webkitFlexFlow = 'wrap row';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
flexbox.style.webkitFlexFlow = 'column row';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');

successfullyParsed = true;
