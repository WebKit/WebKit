// pri-4 variables are used but not defined, so background-color and
// border-color will fall back to the default variables.
const additionalStyleSheet = `
    :host([data-priority="0"]) > li {
        --priority-background-color: var(--complex-todo-red-pri-0);
        --priority-background-color-completed: var(--complex-todo-green-pri-0);
    }
    :host([data-priority="1"]) > li {
        --priority-background-color: var(--complex-todo-red-pri-1);
        --priority-background-color-completed: var(--complex-todo-green-pri-1);
    }
    :host([data-priority="2"]) > li {
        --priority-background-color: var(--complex-todo-red-pri-2);
        --priority-background-color-completed: var(--complex-todo-green-pri-2);
    }
    :host([data-priority="3"]) > li {
        --priority-background-color: var(--complex-todo-red-pri-3);
        --priority-background-color-completed: var(--complex-todo-green-pri-3);
    }
    :host([data-priority="4"]) > li {
        --priority-background-color: var(--complex-todo-red-pri-4);
        --priority-background-color-completed: var(--complex-todo-green-pri-4);
    }
    .todo-item {
        background-color: var(--priority-background-color, var(--complex-background-color-default));
        border-bottom-color: var(--complex-todo-red-border, var(--complex-border-bottom-color-default));
    }
    :host([completed="true"]) > .todo-item {
        background-color: var(--priority-background-color-completed, var(--complex-background-color-default));
        border-bottom-color: var(--complex-todo-green-border, var(--complex-border-bottom-color-default));
    }
    .todo-item > div label {
        color: var(--complex-todo-red-label, --complex-label-color-default);
    }
    :host([completed="true"]) > .todo-item label  {
        color: var(--complex-todo-green-label, --complex-label-color-default);
    }
    .todo-item  > div > :focus,
    .todo-item  > div > .toggle:focus + label {
        box-shadow: var(--complex-box-shadow-red) !important;
    }
    :host([completed="true"]) > .todo-item > div > :focus,
    :host([completed="true"]) > .todo-item > div > .toggle:focus + label {
        box-shadow: var(--complex-box-shadow-green) !important;
    }`;

window.extraTodoItemCssToAdopt = additionalStyleSheet;
