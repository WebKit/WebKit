const sheet = new CSSStyleSheet();
sheet.replaceSync(`:host {
    display: block;
    box-shadow: none !important;
}

.todo-list {
    margin: 0;
    padding: 0;
    list-style: none;
    display: block;
    border-top: 1px solid #e6e6e6;
}
`);
export default sheet;
