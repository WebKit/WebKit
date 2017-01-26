
describe('EditableText', () => {
    const scripts = ['instrumentation.js', 'components/base.js', 'components/editable-text.js'];

    function waitToRender(context)
    {
        if (!context._dummyComponent) {
            const ComponentBase = context.symbols.ComponentBase;
            context._dummyComponent = class SomeComponent extends ComponentBase {
                constructor(resolve)
                {
                    super();
                    this._resolve = resolve;
                }
                render() { setTimeout(this._resolve, 0); }
            }
            ComponentBase.defineElement('dummy-component', context._dummyComponent);
        }
        return new Promise((resolve) => {
            const instance = new context._dummyComponent(resolve);
            context.document.body.appendChild(instance.element());
            instance.enqueueToRender();
        });
    }

    it('show the set text', () => {
        const context = new BrowsingContext();
        let editableText;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.enqueueToRender();
            return waitToRender(context);
        }).then(() => {
            expect(editableText.content().textContent).toNotInclude('hello');
            editableText.setText('hello');
            editableText.enqueueToRender();
            return waitToRender(context);
        }).then(() => {
            expect(editableText.content().textContent).toInclude('hello');
        });
    });

    it('go to the editing mode', () => {
        const context = new BrowsingContext();
        let editableText;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.setText('hello');
            editableText.enqueueToRender();
            return waitToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).toBe(0);
            expect(editableText.content().textContent).toInclude('hello');
            expect(editableText.content().querySelector('a').textContent).toInclude('Edit');
            expect(editableText.content().querySelector('a').textContent).toNotInclude('Save');
            editableText.content().querySelector('a').click();
            return waitToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).toNotBe(0);
            expect(editableText.content().querySelector('a').textContent).toNotInclude('Edit');
            expect(editableText.content().querySelector('a').textContent).toInclude('Save');
        });
    });

    it('should dispatch "update" action', () => {
        const context = new BrowsingContext();
        let editableText;
        let updateCount = 0;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.setText('hello');
            editableText.enqueueToRender();
            editableText.listenToAction('update', () => updateCount++);
            return waitToRender(context);
        }).then(() => {
            editableText.content().querySelector('a').click();
            return waitToRender(context);
        }).then(() => {
            const input = editableText.content().querySelector('input');
            expect(input.offsetHeight).toNotBe(0);
            expect(editableText.editedText()).toBe('hello');
            input.value = 'world';
            expect(editableText.editedText()).toBe('world');
            expect(updateCount).toBe(0);
            editableText.content().querySelector('a').click();
            expect(updateCount).toBe(1);
            expect(editableText.editedText()).toBe('world');
            expect(editableText.text()).toBe('hello');
        });
    });

    it('should end the editing mode when it loses the focus', () => {
        const context = new BrowsingContext();
        let editableText;
        let updateCount = 0;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.setText('hello');
            editableText.enqueueToRender();
            editableText.listenToAction('update', () => updateCount++);
            return waitToRender(context);
        }).then(() => {
            editableText.content().querySelector('a').click();
            return waitToRender(context);
        }).then(() => {
            const input = editableText.content().querySelector('input');
            expect(input.offsetHeight).toNotBe(0);
            expect(editableText.editedText()).toBe('hello');
            input.value = 'world';
            expect(updateCount).toBe(0);

            const focusableElement = document.createElement('div');
            focusableElement.setAttribute('tabindex', 0);
            document.body.appendChild(focusableElement);
            focusableElement.focus();

            return waitToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).toBe(0);
            expect(editableText.text()).toBe('hello');
            expect(updateCount).toBe(0);
        });
    });

    it('should not end the editing mode when the "Save" button is focused', () => {
        const context = new BrowsingContext();
        let editableText;
        let updateCount = 0;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.setText('hello');
            editableText.enqueueToRender();
            editableText.listenToAction('update', () => updateCount++);
            return waitToRender(context);
        }).then(() => {
            editableText.content().querySelector('a').click();
            return waitToRender(context);
        }).then(() => {
            const input = editableText.content().querySelector('input');
            expect(input.offsetHeight).toNotBe(0);
            expect(editableText.editedText()).toBe('hello');
            input.value = 'world';
            expect(updateCount).toBe(0);
            editableText.content().querySelector('a').focus();
            return waitToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).toNotBe(0);
            editableText.content().querySelector('a').click();
            expect(editableText.editedText()).toBe('world');
            expect(updateCount).toBe(1);
        });
    });

    it('should dipsatch "update" action when the "Save" button is clicked in Safari', () => {
        const context = new BrowsingContext();
        let editableText;
        let updateCount = 0;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.setText('hello');
            editableText.enqueueToRender();
            editableText.listenToAction('update', () => updateCount++);
            return waitToRender(context);
        }).then(() => {
            editableText.content('action-button').click();
            return waitToRender(context);
        }).then(() => {
            const input = editableText.content('text-field');
            expect(input.offsetHeight).toNotBe(0);
            expect(editableText.editedText()).toBe('hello');
            input.value = 'world';
            expect(updateCount).toBe(0);
            return waitToRender(context);
        }).then(() => {
            editableText.content('action-button').dispatchEvent(new MouseEvent('mousedown'));
            return new Promise((resolve) => setTimeout(resolve, 0));
        }).then(() => {
            editableText.content('text-field').blur();
            editableText.content('action-button').dispatchEvent(new MouseEvent('mouseup'));
            return waitToRender(context);
        }).then(() => {
            expect(editableText.content('text-field').offsetHeight).toBe(0);
            expect(updateCount).toBe(1);
            expect(editableText.editedText()).toBe('world');
        });
    });

});
