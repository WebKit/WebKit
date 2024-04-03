import { Component } from "react";
import PropTypes from "prop-types";
import TextInput from "./text-input";

export default class Header extends Component {
    static propTypes = {
        addTodo: PropTypes.func.isRequired,
    };

    handleSave = (text) => {
        if (text.length !== 0)
            this.props.addTodo(text);
    };

    render() {
        return (
            <header className="header" data-testid="header">
                <h1>todos</h1>
                <TextInput newTodo onSave={this.handleSave} placeholder="What needs to be done?" />
            </header>
        );
    }
}
