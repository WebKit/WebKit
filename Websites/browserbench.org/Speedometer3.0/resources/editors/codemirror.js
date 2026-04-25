// https://codemirror.net/examples/bundle/
import { EditorView, basicSetup } from "codemirror";
import { StateEffect } from "@codemirror/state";
import { javascript } from "@codemirror/lang-javascript";

let lang = javascript();
let extensions = [basicSetup, EditorView.lineWrapping];

export default function (element, value) {
    let view = new EditorView({
        extensions,
        parent: element,
        doc: value,
        wordWrapColumn: 80,
    });

    return {
        editor: view,
        // Anything before this promise resolves will happen before timing starts
        ready: Promise.resolve(),
        getScrollHeight() {
            return element.scrollHeight;
        },
        getScrollTop() {
            return element.scrollTop;
        },
        setScrollTop(value) {
            element.scrollTop = value;
        },
        setValue: (value) =>
            view.dispatch({
                changes: { from: 0, to: view.state.doc.length, insert: value },
            }),
        format(on) {
            // https://codemirror.net/examples/config/
            // https://discuss.codemirror.net/t/cm6-dynamically-switching-syntax-theme-w-reconfigure/2858/6
            if (on && extensions.length === 2)
                extensions.push(lang);
            else if (!on && extensions.length === 3)
                extensions.pop();

            view.dispatch({
                effects: StateEffect.reconfigure.of(extensions),
            });
        },
    };
}
