'use strict';


const htmlEscapes = {
    '&': '&amp',
    '<': '&lt',
    '>': '&gt',
    '"': '&quot',
    '\'': '&#x27',
    '`': '&#x60'
};

const reUnescapedHtml = /[&<>"'`]/g;
const reHasUnescapedHtml = new RegExp(reUnescapedHtml.source);

let escape = str => (str && reHasUnescapedHtml.test(str)) ? str.replace(reUnescapedHtml, escapeHtmlChar) : str;
let escapeHtmlChar = chr => htmlEscapes[chr];

class Template {
    constructor() {
        this.defaultTemplate = `
            <li data-id="{{id}}" class="{{completed}}">
                <div class="view">
                    <input class="toggle" type="checkbox" {{checked}}>
                    <label>{{title}}</label>
                    <button class="destroy"></button>
                </div>
            </li>
        `;
    }

    /**
     * Creates an <li> HTML string and returns it for placement in your app.
     *
     * NOTE: In real life you should be using a templating engine such as Mustache
     * or Handlebars, however, this is a vanilla JS example.
     *
     * @param {object} data The object containing keys you want to find in the
     *                      template to replace.
     * @returns {string} HTML String of an <li> element
     *
     * @example
     * view.show({
         *  id: 1,
         *  title: "Hello World",
         *  completed: 0,
         * })
     */
    show(data){
        let i = 0;
        let view = '';
        const len = data.length;

        for (i; i < len; i++) {
            let completed = '';
            let checked = '';
            let template = this.defaultTemplate;

            if (data[i].completed) {
                completed = 'completed';
                checked = 'checked';
            }

            template = template.replace('{{id}}', data[i].id);
            template = template.replace('{{title}}', escape(data[i].title));
            template = template.replace('{{completed}}', completed);
            template = template.replace('{{checked}}', checked);

            view += template;
        }

        return view;
    }

    /**
     * Displays a counter of how many to dos are left to complete
     *
     * @param {number} activeTodos The number of active todos.
     * @returns {string} String containing the count
     */
    itemCounter(activeTodos){
        let plural = activeTodos === 1 ? '' : 's';
        return `<strong>${activeTodos}</strong> item${plural} left`;
    }

    /**
     * Updates the text within the "Clear completed" button
     *
     * @param  {[type]} completedTodos The number of completed todos.
     * @returns {string} String containing the count
     */
    clearCompletedButton(completedTodos){
        return (completedTodos > 0) ? 'Clear completed' : '';
    }
}
