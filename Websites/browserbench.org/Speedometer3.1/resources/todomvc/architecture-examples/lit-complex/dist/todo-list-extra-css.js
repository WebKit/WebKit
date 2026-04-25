// pri-4 variables are purposely not defined to test the fallback to the default variables.
const additionalTodoListStyleSheet = `:host(.show-priority) {
    --complex-todo-red-pri-0: rgb(253, 204, 204);
    --complex-todo-red-pri-1: rgb(248, 221, 221);
    --complex-todo-red-pri-2: rgb(251, 234, 234);
    --complex-todo-red-pri-3: rgb(250, 240, 240);
    --complex-todo-red-border: rgb(255, 215, 215);
    --complex-todo-red-label: rgb(37, 0, 0);
    --complex-todo-green-pri-0: rgb(204, 253, 204);
    --complex-todo-green-pri-1: rgb(221, 248, 221);
    --complex-todo-green-pri-2: rgb(234, 251, 234);
    --complex-todo-green-pri-3: rgb(241, 250, 240);
    --complex-todo-green-border: rgb(215, 255, 215);
    --complex-todo-green-label: rgb(135, 167, 144);
    --complex-box-shadow-red: rgb(193, 133, 133) 0 0 2px 2px inset;
    --complex-box-shadow-green: rgb(125, 207, 137) 0 0 2px 2px inset;
}`;

window.extraTodoListCssToAdopt = additionalTodoListStyleSheet;
