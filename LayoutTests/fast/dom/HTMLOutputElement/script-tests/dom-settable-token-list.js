// This test mostly comes from fast/dom/HTMLElement/script-tests/class-list.js
description('Tests the htmlFor attribute and its properties.');

var container = document.createElement('div');
document.body.appendChild(container);

var element;

function createElement(tokenList)
{
    container.innerHTML = '<output for="' + tokenList + '"></output>';
    element = container.lastChild;
}

debug('- Tests from http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/');

// Setting a value to a DOMSettableTokenList should be forwared to the 'value' attribute.
createElement('x');
element.htmlFor = 'y';
shouldBeEqualToString('String(element.htmlFor)', 'y');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/001.htm
createElement('');
shouldEvaluateTo('element.htmlFor.length', 0);

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/002.htm
createElement('x');
shouldEvaluateTo('element.htmlFor.length', 1);

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/003.htm
createElement('x x');
shouldEvaluateTo('element.htmlFor.length', 2);

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/004.htm
createElement('x y');
shouldEvaluateTo('element.htmlFor.length', 2);

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/005.htm
createElement('');
element.htmlFor.add('x');
shouldBeEqualToString('element.htmlFor.toString()', 'x');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/006.htm
createElement('x');
element.htmlFor.add('x');
shouldBeEqualToString('element.htmlFor.toString()', 'x');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/007.htm
createElement('x  x');
element.htmlFor.add('x');
shouldBeEqualToString('element.htmlFor.toString()', 'x  x');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/008.htm
createElement('y');
element.htmlFor.add('x');
shouldBeEqualToString('element.htmlFor.toString()', 'y x');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/009.htm
createElement('');
element.htmlFor.remove('x');
shouldBeEqualToString('element.htmlFor.toString()', '');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/010.htm
createElement('x');
element.htmlFor.remove('x');
shouldBeEqualToString('element.htmlFor.toString()', '');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/011.htm
createElement(' y x  y ');
element.htmlFor.remove('x');
shouldBeEqualToString('element.htmlFor.toString()', ' y y ');

// http://simon.html5.org/test/html/dom/reflecting/DOMTokenList/getting/012.htm
createElement(' x y  x ');
element.htmlFor.remove('x');
shouldBeEqualToString('element.htmlFor.toString()', 'y');


debug('- Ensure that we can handle empty form attribute correctly');
element = document.createElement('output');
var list = element.htmlFor;
list.toggle('x');
shouldBeEqualToString('list.value', 'x');

list.toggle('x');
shouldBeEqualToString('list.value', '');

element = document.createElement('output');
shouldBeFalse('element.htmlFor.contains(\'x\')');
shouldBeUndefined('element.htmlFor[1]');
element.htmlFor.remove('x');
element.htmlFor.add('x')
shouldBeTrue('element.htmlFor.contains(\'x\')');
shouldBeUndefined('element.htmlFor[1]');

debug('- Testing add in presence of trailing white spaces.');

createElement('x ');
element.htmlFor.add('y');
shouldBeEqualToString('element.htmlFor.toString()', 'x y');

createElement('x\t');
element.htmlFor.add('y');
shouldBeEqualToString('element.htmlFor.toString()', 'x\ty');

createElement(' ');
element.htmlFor.add('y');
shouldBeEqualToString('element.htmlFor.toString()', ' y');


debug('- Test invalid tokens');

// Testing exception due to invalid token

// shouldThrow from js-test-pre.js is not sufficient.
function shouldThrowDOMException(f, ec)
{
    try {
        f();
        testFailed('Expected an exception');
    } catch (ex) {
        if (!(ex instanceof DOMException)) {
            testFailed('Exception is not an instance of DOMException, found: ' +
                       Object.toString.call(ex));
            return;
        }
        if (ec !== ex.code) {
            testFailed('Wrong exception code: ' + ex.code);
            return;
        }
    }
    var formattedFunction = String(f).replace(/^function.+\{\s*/m, '').
        replace(/;?\s+\}/m, '');
    testPassed(formattedFunction + ' threw expected DOMException with code ' + ec);
}

createElement('x');
shouldThrowDOMException(function() {
    element.htmlFor.contains('');
}, DOMException.SYNTAX_ERR);

createElement('x y');
shouldThrowDOMException(function() {
    element.htmlFor.contains('x y');
}, DOMException.INVALID_CHARACTER_ERR);

createElement('');
shouldThrowDOMException(function() {
    element.htmlFor.add('');
}, DOMException.SYNTAX_ERR);

createElement('');
shouldThrowDOMException(function() {
    element.htmlFor.add('x y');
}, DOMException.INVALID_CHARACTER_ERR);

createElement('');
shouldThrowDOMException(function() {
    element.htmlFor.remove('');
}, DOMException.SYNTAX_ERR);

createElement('');
shouldThrowDOMException(function() {
    element.htmlFor.remove('x y');
}, DOMException.INVALID_CHARACTER_ERR);

createElement('');
shouldThrowDOMException(function() {
    element.htmlFor.toggle('');
}, DOMException.SYNTAX_ERR);

createElement('x y');
shouldThrowDOMException(function() {
    element.htmlFor.toggle('x y');
}, DOMException.INVALID_CHARACTER_ERR);


debug('- Indexing');

createElement('x');
shouldBeEqualToString('element.htmlFor[0]', 'x');
shouldBeEqualToString('element.htmlFor.item(0)', 'x');

createElement('x x');
shouldBeEqualToString('element.htmlFor[1]', 'x');
shouldBeEqualToString('element.htmlFor.item(1)', 'x');

createElement('x y');
shouldBeEqualToString('element.htmlFor[1]', 'y');
shouldBeEqualToString('element.htmlFor.item(1)', 'y');

createElement('');
shouldBeUndefined('element.htmlFor[0]');
shouldBeNull('element.htmlFor.item(0)');

createElement('x y z');
shouldBeUndefined('element.htmlFor[4]');
shouldBeNull('element.htmlFor.item(4)');
shouldBeUndefined('element.htmlFor[-1]');  // Not a valid index so should not trigger item().
shouldBeNull('element.htmlFor.item(-1)');

debug('- Test case since DOMTokenList is case sensitive');

createElement('x');
shouldBeTrue('element.htmlFor.contains(\'x\')');
shouldBeFalse('element.htmlFor.contains(\'X\')');
shouldBeEqualToString('element.htmlFor[0]', 'x');

createElement('X');
shouldBeTrue('element.htmlFor.contains(\'X\')');
shouldBeFalse('element.htmlFor.contains(\'x\')');
shouldBeEqualToString('element.htmlFor[0]', 'X');


debug('- Testing whitespace');
// U+0020 SPACE, U+0009 CHARACTER TABULATION (tab), U+000A LINE FEED (LF),
// U+000C FORM FEED (FF), and U+000D CARRIAGE RETURN (CR)

createElement('x\u0020y');
shouldEvaluateTo('element.htmlFor.length', 2);

createElement('x\u0009y');
shouldEvaluateTo('element.htmlFor.length', 2);

createElement('x\u000Ay');
shouldEvaluateTo('element.htmlFor.length', 2);

createElement('x\u000Cy');
shouldEvaluateTo('element.htmlFor.length', 2);

createElement('x\u000Dy');
shouldEvaluateTo('element.htmlFor.length', 2);


debug('- DOMSettableTokenList presence and type');


// Safari returns object
// Firefox returns object
// IE8 returns object
// Chrome returns function
// assertEquals('object', typeof DOMSettableTokenList);
shouldBeTrue('\'undefined\' != typeof DOMSettableTokenList');

shouldBeEqualToString('typeof DOMSettableTokenList.prototype', 'object');

createElement('x');
shouldBeEqualToString('typeof element.htmlFor', 'object');

shouldEvaluateTo('element.htmlFor.constructor', 'DOMSettableTokenList');

shouldBeTrue('element.htmlFor === element.htmlFor');
