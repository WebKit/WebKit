const sheet = new CSSStyleSheet();
sheet.replaceSync(`:host {
    display: block;
    box-shadow: none !important;
}

.topbar {
    position: relative;
}

.new-todo-input {
    padding: 0 32px 0 60px;
    width: 100%;
    height: 68px;
    font-size: 24px;
    line-height: 1.4em;
    background: rgba(0, 0, 0, 0.003);
    box-shadow: inset 0 -2px 1px rgba(0, 0, 0, 0.03);
}

.new-todo-input::placeholder {
    font-style: italic;
    font-weight: 400;
    color: rgba(0, 0, 0, 0.4);
}

.toggle-all-container {
    width: 45px;
    height: 68px;
    position: absolute;
    left: 0;
    top: 0;
}

.toggle-all-input {
    width: 45px;
    height: 45px;
    font-size: 0;
    position: absolute;
    top: 11.5px;
    left: 0;
    border: none;
    appearance: none;
    cursor: pointer;
}

.toggle-all-label {
    display: flex;
    align-items: center;
    justify-content: center;
    width: 45px;
    height: 68px;
    font-size: 0;
    position: absolute;
    top: 0;
    left: 0;
    cursor: pointer;
}

.toggle-all-label::before {
    content: "❯";
    display: inline-block;
    font-size: 22px;
    color: #949494;
    padding: 10px 27px 10px 27px;
    transform: rotate(90deg);
}

.toggle-all-input:checked + .toggle-all-label::before {
    color: #484848;
}

@media screen and (-webkit-min-device-pixel-ratio: 0) {
    .toggle-all-input {
        background: none;
    }
}

html[dir="rtl"] .new-todo-input,
:host([dir="rtl"]) .new-todo-input {
    padding: 0 60px 0 32px;
}

html[dir="rtl"] .toggle-all-container,
:host([dir="rtl"]) .toggle-all-container {
    right: 0;
    left: unset;
}
`);
export default sheet;
