function create_constructor_log(element) {
    return {type: 'constructed', element: element};
}

function assert_constructor_log_entry(log, element) {
    assert_equals(log.type, 'constructed');
    assert_equals(log.element, element);
}

function create_connected_callback_log(element) {
    return {type: 'connected', element: element};
}

function assert_connected_log_entry(log, element) {
    assert_equals(log.type, 'connected');
    assert_equals(log.element, element);
}

function define_custom_element_in_window(window, name, observedAttributes) {
    let log = [];

    class CustomElement extends window.HTMLElement {
        constructor() {
            super();
            log.push({type: 'constructed', element: this});
        }
        attributeChangedCallback(...args) {
            log.push(create_attribute_changed_callback_log(this, ...args));
        }
        connectedCallback() { log.push({type: 'connected', element: this}); }
        disconnectedCallback() { log.push({type: 'disconnected', element: this}); }
        adoptedCallback(oldDocument, newDocument) { log.push({type: 'adopted', element: this, oldDocument: oldDocument, newDocument: newDocument}); }
    }
    CustomElement.observedAttributes = observedAttributes;

    window.customElements.define(name, CustomElement);

    return {
        name: name,
        class: CustomElement,
        takeLog: function () {
            let currentLog = log; log = [];
            currentLog.types = () => currentLog.map((entry) => entry.type);
            currentLog.last = () => currentLog[currentLog.length - 1];
            return currentLog;
        }
    };
}
