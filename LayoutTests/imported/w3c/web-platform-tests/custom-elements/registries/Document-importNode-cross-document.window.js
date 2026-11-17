// Target document has a global registry

test(t => {
  const contentDocument = document.body.appendChild(document.createElement('iframe')).contentDocument;
  t.add_cleanup(() => contentDocument.defaultView.frameElement.remove());

  const element = document.createElement("div");
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, contentDocument.defaultView.customElements);
}, "Cloning with global registry");

test(t => {
  const contentDocument = document.body.appendChild(document.createElement('iframe')).contentDocument;
  t.add_cleanup(() => contentDocument.defaultView.frameElement.remove());

  const element = document.createElement("div", { customElementRegistry: customElements });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, contentDocument.defaultView.customElements);
}, "Cloning with explicit global registry");

test(t => {
  const contentDocument = document.body.appendChild(document.createElement('iframe')).contentDocument;
  t.add_cleanup(() => contentDocument.defaultView.frameElement.remove());

  const scoped = new CustomElementRegistry();
  const element = document.createElement("div", { customElementRegistry: scoped });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, scoped);
}, "Cloning with scoped registry");

test(t => {
  const contentDocument = document.body.appendChild(document.createElement('iframe')).contentDocument;
  t.add_cleanup(() => contentDocument.defaultView.frameElement.remove());

  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, contentDocument.defaultView.customElements);
}, "Cloning including shadow tree with global registry");

test(t => {
  const contentDocument = document.body.appendChild(document.createElement('iframe')).contentDocument;
  t.add_cleanup(() => contentDocument.defaultView.frameElement.remove());

  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true, customElementRegistry: customElements });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, contentDocument.defaultView.customElements);
}, "Cloning including shadow tree with explicit global registry");

test(t => {
  const contentDocument = document.body.appendChild(document.createElement('iframe')).contentDocument;
  t.add_cleanup(() => contentDocument.defaultView.frameElement.remove());

  const scoped = new CustomElementRegistry();
  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true, customElementRegistry: scoped });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, scoped);
}, "Cloning including shadow tree with scoped registry");


// Target document has a null registry

test(t => {
  const contentDocument = document.implementation.createHTMLDocument();

  const element = document.createElement("div");
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, null);
}, "Cloning with global registry (null registry target)");

test(t => {
  const contentDocument = document.implementation.createHTMLDocument();

  const element = document.createElement("div", { customElementRegistry: customElements });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, null);
}, "Cloning with explicit global registry (null registry target)");

test(t => {
  const contentDocument = document.implementation.createHTMLDocument();

  const scoped = new CustomElementRegistry();
  const element = document.createElement("div", { customElementRegistry: scoped });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, scoped);
}, "Cloning with scoped registry (null registry target)");

test(t => {
  const contentDocument = document.implementation.createHTMLDocument();

  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, null);
}, "Cloning including shadow tree with global registry (null registry target)");

test(t => {
  const contentDocument = document.implementation.createHTMLDocument();

  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true, customElementRegistry: customElements });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, null);
}, "Cloning including shadow tree with explicit global registry (null registry target)");

test(t => {
  const contentDocument = document.implementation.createHTMLDocument();

  const scoped = new CustomElementRegistry();
  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true, customElementRegistry: scoped });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, scoped);
}, "Cloning including shadow tree with scoped registry (null registry target)");


// Target document has a scoped registry

function createScopedRegistryDocument() {
  const contentDocument = document.implementation.createHTMLDocument();
  const registry = new CustomElementRegistry();
  registry.initialize(contentDocument);
  return [contentDocument, registry];
}

test(t => {
  const [contentDocument] = createScopedRegistryDocument();

  const element = document.createElement("div");
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, null);
}, "Cloning with global registry (scoped registry target)");

test(t => {
  const [contentDocument] = createScopedRegistryDocument();

  const element = document.createElement("div", { customElementRegistry: customElements });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, null);
}, "Cloning with explicit global registry (scoped registry target)");

test(t => {
  const [contentDocument, registry] = createScopedRegistryDocument();

  const element = document.createElement("div", { customElementRegistry: null });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, registry);
}, "Cloning with null registry (scoped registry target)");

test(t => {
  const [contentDocument] = createScopedRegistryDocument();

  const scoped = new CustomElementRegistry();
  const element = document.createElement("div", { customElementRegistry: scoped });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.customElementRegistry, scoped);
}, "Cloning with scoped registry (scoped registry target)");

test(t => {
  const [contentDocument] = createScopedRegistryDocument();

  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, null);
}, "Cloning including shadow tree with global registry (scoped registry target)");

test(t => {
  const [contentDocument] = createScopedRegistryDocument();

  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true, customElementRegistry: customElements });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, null);
}, "Cloning including shadow tree with explicit global registry (scoped registry target)");

test(t => {
  const [contentDocument] = createScopedRegistryDocument();

  const scoped = new CustomElementRegistry();
  const element = document.createElement("div");
  const elementShadow = element.attachShadow({ mode: "open", clonable: true, customElementRegistry: scoped });
  const clone = contentDocument.importNode(element);
  assert_equals(clone.shadowRoot.customElementRegistry, scoped);
}, "Cloning including shadow tree with scoped registry (scoped registry target)");
