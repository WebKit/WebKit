// The HTML parser gives SVG and MathML elements, and the HTML elements inside them, the custom
// element registry of their parent, as it does for other HTML elements.
// https://html.spec.whatwg.org/#create-an-element-for-the-token

const markup = '<svg><foreignObject><span></span></foreignObject></svg>' +
  '<math><mi><b></b></mi><annotation-xml encoding="text/html"><i></i></annotation-xml></math>';

function assertRegistries(root, expected) {
  const elements = root.querySelectorAll('*');
  assert_equals(elements.length, 8);
  for (const element of elements)
    assert_equals(element.customElementRegistry, expected, element.localName);
}

for (const method of ['setHTMLUnsafe', 'innerHTML']) {
  test(() => {
    const registry = new CustomElementRegistry();
    const host = document.createElement('div', {customElementRegistry: registry});
    if (method === 'innerHTML')
      host.innerHTML = markup;
    else
      host.setHTMLUnsafe(markup);
    assertRegistries(host, registry);
  }, `${method} gives SVG and MathML elements the scoped registry of their parent`);

  test(() => {
    const host = document.createElement('div', {customElementRegistry: null});
    if (method === 'innerHTML')
      host.innerHTML = markup;
    else
      host.setHTMLUnsafe(markup);
    assertRegistries(host, null);
  }, `${method} gives SVG and MathML elements the null registry of their parent`);

  test(() => {
    const registry = new CustomElementRegistry();
    const shadowRoot = document.createElement('div').attachShadow({mode: 'open', customElementRegistry: registry});
    if (method === 'innerHTML')
      shadowRoot.innerHTML = markup;
    else
      shadowRoot.setHTMLUnsafe(markup);
    assertRegistries(shadowRoot, registry);
  }, `${method} gives SVG and MathML elements in a shadow root its scoped registry`);
}

test(() => {
  const registry = new CustomElementRegistry();
  class ScopedElement extends HTMLElement {}
  registry.define('scoped-element', ScopedElement);
  const host = document.createElement('div', {customElementRegistry: registry});
  host.setHTMLUnsafe('<svg><foreignObject><scoped-element></scoped-element></foreignObject></svg>');
  const element = host.querySelector('scoped-element');
  assert_equals(element.customElementRegistry, registry);
  assert_true(element instanceof ScopedElement);
}, 'A custom element inside foreignObject is upgraded with the scoped registry of its context');
