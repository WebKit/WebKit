// https://tiptap.dev/examples/default
import { Editor } from "@tiptap/core";
import StarterKit from "@tiptap/starter-kit";

export default function (element, value) {
    let editor = new Editor({
        element,
        extensions: [StarterKit],
        content: value,
        editorProps: {
            attributes: {
                spellcheck: "false",
            },
        },
    });
    return {
        editor,
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
        setValue(value) {
            // Recommendation is to call focus before most commands
            // https://tiptap.dev/api/commands#chain-commands
            editor.chain().focus().setContent(value).setTextSelection(0).run();
            element.scrollTop = 0;
        },
        format(on) {
            if (on)
                editor.chain().focus().selectAll().setBold().setTextSelection(0).run();
            else
                editor.chain().focus().selectAll().unsetBold().setTextSelection(0).run();
        },
    };
}
