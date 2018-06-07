
describe('EditableText', () => {
    const scripts = ['instrumentation.js', '../shared/common-component-base.js', 'components/base.js', 'components/editable-text.js'];

    it('show the set text', () => {
        const context = new BrowsingContext();
        let editableText;
        return context.importScripts(scripts, 'EditableText', 'ComponentBase').then((symbols) => {
            const [EditableText] = symbols;
            editableText = new EditableText;
            context.document.body.appendChild(editableText.element());
            editableText.enqueueToRender();
            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content().textContent).to.not.contain('hello');
            editableText.setText('hello');
            editableText.enqueueToRender();
            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content().textContent).to.contain('hello');
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
            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).to.be(0);
            expect(editableText.content().textContent).to.contain('hello');
            expect(editableText.content().querySelector('a').textContent).to.contain('Edit');
            expect(editableText.content().querySelector('a').textContent).to.not.contain('Save');
            editableText.content().querySelector('a').click();
            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).to.not.be(0);
            expect(editableText.content().querySelector('a').textContent).to.not.contain('Edit');
            expect(editableText.content().querySelector('a').textContent).to.contain('Save');
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
            return waitForComponentsToRender(context);
        }).then(() => {
            editableText.content().querySelector('a').click();
            return waitForComponentsToRender(context);
        }).then(() => {
            const input = editableText.content().querySelector('input');
            expect(input.offsetHeight).to.not.be(0);
            expect(editableText.editedText()).to.be('hello');
            input.value = 'world';
            expect(editableText.editedText()).to.be('world');
            expect(updateCount).to.be(0);
            editableText.content().querySelector('a').click();
            expect(updateCount).to.be(1);
            expect(editableText.editedText()).to.be('world');
            expect(editableText.text()).to.be('hello');
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
            return waitForComponentsToRender(context);
        }).then(() => {
            editableText.content().querySelector('a').click();
            return waitForComponentsToRender(context);
        }).then(() => {
            const input = editableText.content().querySelector('input');
            expect(input.offsetHeight).to.not.be(0);
            expect(editableText.editedText()).to.be('hello');
            input.value = 'world';
            expect(updateCount).to.be(0);

            const focusableElement = document.createElement('div');
            focusableElement.setAttribute('tabindex', 0);
            document.body.appendChild(focusableElement);
            focusableElement.focus();

            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).to.be(0);
            expect(editableText.text()).to.be('hello');
            expect(updateCount).to.be(0);
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
            return waitForComponentsToRender(context);
        }).then(() => {
            editableText.content().querySelector('a').click();
            return waitForComponentsToRender(context);
        }).then(() => {
            const input = editableText.content().querySelector('input');
            expect(input.offsetHeight).to.not.be(0);
            expect(editableText.editedText()).to.be('hello');
            input.value = 'world';
            expect(updateCount).to.be(0);
            editableText.content().querySelector('a').focus();
            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content().querySelector('input').offsetHeight).to.not.be(0);
            editableText.content().querySelector('a').click();
            expect(editableText.editedText()).to.be('world');
            expect(updateCount).to.be(1);
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
            return waitForComponentsToRender(context);
        }).then(() => {
            editableText.content('action-button').click();
            return waitForComponentsToRender(context);
        }).then(() => {
            const input = editableText.content('text-field');
            expect(input.offsetHeight).to.not.be(0);
            expect(editableText.editedText()).to.be('hello');
            input.value = 'world';
            expect(updateCount).to.be(0);
            return waitForComponentsToRender(context);
        }).then(() => {
            editableText.content('action-button').dispatchEvent(new MouseEvent('mousedown'));
            return wait(0);
        }).then(() => {
            editableText.content('text-field').blur();
            editableText.content('action-button').dispatchEvent(new MouseEvent('mouseup'));
            return waitForComponentsToRender(context);
        }).then(() => {
            expect(editableText.content('text-field').offsetHeight).to.be(0);
            expect(updateCount).to.be(1);
            expect(editableText.editedText()).to.be('world');
        });
    });

});
