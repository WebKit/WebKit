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


shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', '');
// The initial value is 'stretch'.
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'stretch');

flexbox.style.webkitFlexItemAlign = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', '');

flexbox.style.webkitFlexItemAlign = 'auto';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', 'auto');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'stretch');

flexbox.style.webkitFlexItemAlign = 'start';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', 'start');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'start');

flexbox.style.webkitFlexItemAlign = 'end';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', 'end');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'end');

flexbox.style.webkitFlexItemAlign = 'center';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', 'center');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'center');

flexbox.style.webkitFlexItemAlign = 'stretch';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'stretch');

flexbox.style.webkitFlexItemAlign = 'baseline';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', 'baseline');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'baseline');

flexbox.style.webkitFlexItemAlign = '';
shouldBeEqualToString('flexbox.style.webkitFlexItemAlign', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexItemAlign', 'stretch');

shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');
// The initial value is 'stretch'.
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'stretch');

flexbox.style.webkitFlexAlign = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');

flexbox.style.webkitFlexAlign = 'auto';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'stretch');

flexbox.style.webkitFlexAlign = 'start';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'start');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'start');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'start');

flexbox.style.webkitFlexAlign = 'end';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'end');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'end');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'end');

flexbox.style.webkitFlexAlign = 'center';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'center');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'center');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'center');

flexbox.style.webkitFlexAlign = 'stretch';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'stretch');

flexbox.style.webkitFlexAlign = 'baseline';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', 'baseline');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'baseline');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'baseline');

flexbox.style.webkitFlexAlign = '';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'stretch');

flexbox.style.display = 'none';
shouldBeEqualToString('flexbox.style.webkitFlexAlign', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexAlign', 'stretch');
shouldBeEqualToString('window.getComputedStyle(flexitem, null).webkitFlexItemAlign', 'stretch');
flexbox.style.display = '';


// FIXME: This should probably return stretch. See https://bugs.webkit.org/show_bug.cgi?id=14563.
var detachedFlexbox = document.createElement('div');
var detachedFlexItem = document.createElement('div');
detachedFlexbox.appendChild(detachedFlexItem);
shouldBeEqualToString('window.getComputedStyle(detachedFlexbox, null).webkitFlexItemAlign', '');
shouldBeEqualToString('window.getComputedStyle(detachedFlexItem, null).webkitFlexItemAlign', '');


shouldBeEqualToString('flexbox.style.webkitFlexDirection', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexDirection', 'row');

flexbox.style.webkitFlexDirection = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexDirection', '');

flexbox.style.webkitFlexDirection = 'row';
shouldBeEqualToString('flexbox.style.webkitFlexDirection', 'row');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexDirection', 'row');

flexbox.style.webkitFlexDirection = 'row-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexDirection', 'row-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexDirection', 'row-reverse');

flexbox.style.webkitFlexDirection = 'column';
shouldBeEqualToString('flexbox.style.webkitFlexDirection', 'column');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexDirection', 'column');

flexbox.style.webkitFlexDirection = 'column-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexDirection', 'column-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexDirection', 'column-reverse');

shouldBeEqualToString('flexbox.style.webkitFlexWrap', '');
// The initial value is 'stretch'.
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexWrap', 'nowrap');

flexbox.style.webkitFlexWrap = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexWrap', '');

flexbox.style.webkitFlexWrap = 'nowrap';
shouldBeEqualToString('flexbox.style.webkitFlexWrap', 'nowrap');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexWrap', 'nowrap');

flexbox.style.webkitFlexWrap = 'wrap';
shouldBeEqualToString('flexbox.style.webkitFlexWrap', 'wrap');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexWrap', 'wrap');

flexbox.style.webkitFlexWrap = 'wrap-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexWrap', 'wrap-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexWrap', 'wrap-reverse');

flexbox.style.webkitFlexFlow = '';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row nowrap');

flexbox.style.webkitFlexFlow = 'foo';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');

function testFlexFlowValue(value, expected, expectedComputed)
{
    flexbox.style.webkitFlexFlow = value;
    shouldBeEqualToString('flexbox.style.webkitFlexFlow', expected.replace(/^ /, '').replace(/ $/, ''));
    shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', expectedComputed);
}

var directions = ['', 'row', 'row-reverse', 'column', 'column-reverse'];
var wraps = ['', 'nowrap', 'wrap', 'wrap-reverse'];
directions.forEach(function(direction) {
    wraps.forEach(function(wrap) {
        var expectedComputed = (direction || 'row') + ' ' + (wrap || 'nowrap');
        var expected = direction + ' ' + wrap;
        testFlexFlowValue(direction + ' ' + wrap, expected, expectedComputed);
        testFlexFlowValue(wrap + ' ' + direction, expected, expectedComputed);
    });
});

flexbox.style.webkitFlexFlow = '';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
flexbox.style.webkitFlexFlow = 'wrap wrap-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
flexbox.style.webkitFlexFlow = 'column row';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');

flexbox.style.webkitFlexFlow = '';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', '');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row nowrap');
flexbox.style.webkitFlexDirection = 'column';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'column');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'column nowrap');
flexbox.style.webkitFlexWrap = 'wrap';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'column wrap');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'column wrap');
flexbox.style.webkitFlexFlow = 'row-reverse wrap-reverse';
shouldBeEqualToString('flexbox.style.webkitFlexFlow', 'row-reverse wrap-reverse');
shouldBeEqualToString('window.getComputedStyle(flexbox, null).webkitFlexFlow', 'row-reverse wrap-reverse');


successfullyParsed = true;
