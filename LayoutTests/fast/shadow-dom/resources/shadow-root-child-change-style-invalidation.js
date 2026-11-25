const red = 'rgb(255, 0, 0)';
const green = 'rgb(0, 128, 0)';

// The style elements come first or last, wherever they don't change the positions being tested.
function makeHost(shadowMarkup) {
    const host = document.createElement('div');
    host.innerHTML = '<span>light</span>';
    document.body.appendChild(host);
    const shadowRoot = host.attachShadow({ mode: 'open' });
    shadowRoot.innerHTML = shadowMarkup;
    document.body.offsetHeight;
    return { host, shadowRoot };
}

// Inserting or removing a child of a shadow root invalidates only the elements whose style may depend on it,
// never the whole host subtree, which includes the slotted light DOM.
function testInvalidation(name, shadowMarkup, mutate, invalidatedSelector) {
    test(() => {
        assert_true(!!window.internals, 'This test requires window.internals');
        const { host, shadowRoot } = makeHost(shadowMarkup);
        internals.updateLayoutIgnorePendingStylesheetsAndRunPostLayoutTasks();
        mutate(shadowRoot);
        if (invalidatedSelector == ':host')
            assert_not_equals(internals.styleChangeType(host), 'NoStyleChange', 'host');
        else {
            assert_equals(internals.styleChangeType(host), 'NoStyleChange', 'host');
            if (invalidatedSelector)
                assert_not_equals(internals.styleChangeType(shadowRoot.querySelector(invalidatedSelector)), 'NoStyleChange', invalidatedSelector);
        }
        assert_equals(internals.styleChangeType(host.firstChild), 'NoStyleChange', 'slotted child');
        host.remove();
    }, name);
}

function testStyle(name, shadowMarkup, mutate, targetSelector, color) {
    test(() => {
        const { host, shadowRoot } = makeHost(shadowMarkup);
        const target = targetSelector == ':host' ? host : shadowRoot.querySelector(targetSelector);
        assert_equals(getComputedStyle(target).color, red, 'before');
        mutate(shadowRoot);
        assert_equals(getComputedStyle(target).color, color, 'after');
        host.remove();
    }, name);
}

const appendDiv = shadowRoot => shadowRoot.append(document.createElement('div'));
const prependDiv = shadowRoot => shadowRoot.prepend(document.createElement('div'));
const prependA = shadowRoot => shadowRoot.prepend(Object.assign(document.createElement('div'), { className: 'a' }));
const appendX = shadowRoot => shadowRoot.append(Object.assign(document.createElement('div'), { className: 'x' }));
const removeOther = shadowRoot => shadowRoot.getElementById('other').remove();

testInvalidation('Appending an element without sibling-dependent rules invalidates nothing else',
    '<slot></slot><div id="a"></div><style>div { color: green }</style>', appendDiv, null);
testInvalidation('Removing an element without sibling-dependent rules invalidates nothing else',
    '<slot></slot><div id="other"></div><style>div { color: green }</style>', removeOther, null);
testInvalidation(':first-child invalidates the previous first child',
    '<div id="a"></div><slot></slot><style>:first-child { color: green }</style>', prependDiv, '#a');
testInvalidation(':last-child invalidates the previous last child',
    '<style>:last-child { color: green }</style><slot></slot><div id="a"></div>', appendDiv, '#a');
testInvalidation('Direct sibling combinator invalidates the next sibling',
    '<div class="b"></div><slot></slot><style>.a + .b { color: green }</style>', prependA, '.b');
testInvalidation('Indirect sibling combinator invalidates the following siblings',
    '<div class="b"></div><slot></slot><style>.a ~ .b { color: green }</style>', prependA, '.b');
testInvalidation(':nth-child() invalidates the following siblings',
    '<div id="a"></div><slot></slot><style>:host > :nth-child(2) { color: green }</style>', prependDiv, '#a');
testInvalidation(':nth-last-child() invalidates the preceding siblings',
    '<div id="a"></div><slot></slot><style>:host > :nth-last-child(2) { color: green }</style>', appendDiv, '#a');
testInvalidation(':host:has() invalidates the host',
    '<slot></slot><style>:host:has(.x) { color: green }</style>', appendX, ':host');

testStyle('Direct sibling combinator matches after inserting the previous sibling',
    '<div class="b"></div><style>.b { color: red } .a + .b { color: green }</style>', prependA, '.b', green);
testStyle(':first-child matches after moving the previous sibling to the end',
    '<div id="other"></div><div id="target"></div><style>div { color: red } :first-child { color: green }</style>',
    shadowRoot => shadowRoot.moveBefore(shadowRoot.getElementById('other'), null), '#target', green);
testStyle(':host:has() matches after appending a matching child',
    '<style>:host { color: red } :host:has(.x) { color: green }</style><slot></slot>', appendX, ':host', green);
testStyle('Positional ancestor in a selector list matches after removing the previous sibling',
    '<div id="other"></div><div><span id="target"></span></div><style>span { color: red } :is(:first-child *) { color: green }</style>',
    removeOther, '#target', green);
testStyle('Positional @scope root matches after removing the previous sibling',
    '<div id="other"></div><div><span id="target"></span></div><style>span { color: red } @scope (:first-child) { span { color: green } }</style>',
    removeOther, '#target', green);
