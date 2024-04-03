const sheet = new CSSStyleSheet();
sheet.replaceSync(`:host {
    display: block;
    box-shadow: none !important;
}

:host(:last-child) > .todo-item {
    border-bottom: none;
}

.todo-item {
    position: relative;
    font-size: 24px;
    border-bottom: 1px solid #ededed;
    height: 60px;
}

.todo-item.editing {
    border-bottom: none;
    padding: 0;
}

.edit-todo-container {
    display: none;
}

.todo-item.editing .edit-todo-container {
    display: block;
}

.edit-todo-input {
    padding: 0 16px 0 60px;
    width: 100%;
    height: 60px;
    font-size: 24px;
    line-height: 1.4em;
    background: rgba(0, 0, 0, 0.003);
    box-shadow: inset 0 -2px 1px rgba(0, 0, 0, 0.03);
    background-image: url("data:image/svg+xml;utf8,%3Csvg%20xmlns%3D%22http%3A//www.w3.org/2000/svg%22%20width%3D%2240%22%20height%3D%2240%22%20%20style%3D%22opacity%3A%200.2%22%20viewBox%3D%22-10%20-18%20100%20135%22%3E%3Ccircle%20cx%3D%2250%22%20cy%3D%2250%22%20r%3D%2250%22%20fill%3D%22none%22%20stroke%3D%22%23949494%22%20stroke-width%3D%223%22/%3E%3C/svg%3E");
    background-repeat: no-repeat;
    background-position: center left;
}

.display-todo {
    position: relative;
}

.todo-item.editing .display-todo {
    display: none;
}

.toggle-todo-input {
    text-align: center;
    width: 40px;

        height: auto;
    position: absolute;
    top: 0;
    bottom: 0;
    left: 3px;
    margin: auto 0;
    border: none;     appearance: none;
    cursor: pointer;
}

.todo-item-text {
    overflow-wrap: break-word;
    padding: 0 60px;
    display: block;
    line-height: 60px;
    transition: color 0.4s;
    font-weight: 400;
    color: #484848;

        background-image: url("data:image/svg+xml;utf8,%3Csvg%20xmlns%3D%22http%3A//www.w3.org/2000/svg%22%20width%3D%2240%22%20height%3D%2240%22%20viewBox%3D%22-10%20-18%20100%20135%22%3E%3Ccircle%20cx%3D%2250%22%20cy%3D%2250%22%20r%3D%2250%22%20fill%3D%22none%22%20stroke%3D%22%23949494%22%20stroke-width%3D%223%22/%3E%3C/svg%3E");
    background-repeat: no-repeat;
    background-position: center left;
}

.toggle-todo-input:checked + .todo-item-text {
    color: #949494;
    text-decoration: line-through;
    background-image: url("data:image/svg+xml;utf8,%3Csvg%20xmlns%3D%22http%3A%2F%2Fwww.w3.org%2F2000%2Fsvg%22%20width%3D%2240%22%20height%3D%2240%22%20viewBox%3D%22-10%20-18%20100%20135%22%3E%3Ccircle%20cx%3D%2250%22%20cy%3D%2250%22%20r%3D%2250%22%20fill%3D%22none%22%20stroke%3D%22%2359A193%22%20stroke-width%3D%223%22%2F%3E%3Cpath%20fill%3D%22%233EA390%22%20d%3D%22M72%2025L42%2071%2027%2056l-4%204%2020%2020%2034-52z%22%2F%3E%3C%2Fsvg%3E");
}

.remove-todo-button {
    display: none;
    position: absolute;
    top: 0;
    right: 10px;
    bottom: 0;
    width: 40px;
    height: 40px;
    margin: auto 0;
    font-size: 30px;
    color: #949494;
    transition: color 0.2s ease-out;
    cursor: pointer;
}

.remove-todo-button:hover,
.remove-todo-button:focus {
    color: #c18585;
}

.remove-todo-button::after {
    content: "×";
    display: block;
    height: 100%;
    line-height: 1.1;
}

.todo-item:hover .remove-todo-button {
    display: block;
}

@media screen and (-webkit-min-device-pixel-ratio: 0) {
    .toggle-todo-input {
        background: none;
        height: 40px;
    }
}

@media (max-width: 430px) {
    .remove-todo-button {
        display: block;
    }
}

html[dir="rtl"] .toggle-todo-input,
:host([dir="rtl"]) .toggle-todo-input {
    right: 3px;
    left: unset;
}

html[dir="rtl"] .todo-item-text,
:host([dir="rtl"]) .todo-item-text {
    background-position: center right 6px;
}

html[dir="rtl"] .remove-todo-button,
:host([dir="rtl"]) .remove-todo-button {
    left: 10px;
    right: unset;
}
`);
export default sheet;
