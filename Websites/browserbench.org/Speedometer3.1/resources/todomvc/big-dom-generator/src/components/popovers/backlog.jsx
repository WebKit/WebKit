import { PopOver } from "./popover";
import { ActionGroup } from "./../action-group";
import classnames from "classnames";

import CheckmarkIcon from "./../../assets/CheckmarkSize100.svg";
import EditIcon from "./../../assets/Smock_Edit_18_N.svg";
import DeleteIcon from "./../../assets/Smock_Delete_18_N.svg";

export const BacklogPopOver = ({ className }) => {
    const listItems = [];
    for (let i = 0; i < 5; i++) {
        listItems.push(
            <li key={i}>
                <div className={classnames("spectrum-Checkbox", "spectrum-Checkbox--sizeM")}>
                    <input type="checkbox" className="spectrum-Checkbox-input" title="Done" />
                    <div className="spectrum-Checkbox-box">
                        <CheckmarkIcon className={classnames("spectrum-Icon", "spectrum-Checkbox-checkmark", "spectrum-UIIcon-Checkmark50")} focusable="false" aria-hidden="true" />
                    </div>
                    <label className="spectrum-Checkbox-label">Task {i}</label>
                    <label className="spectrum-Checkbox-label"> Age </label>
                    <ActionGroup>
                        <button className={classnames("spectrum-Button", "spectrum-Button--fill", "spectrum-Button--primary", "spectrum-Button--sizeS", "spectrum-Button--iconOnly")}>
                            <EditIcon className={classnames("spectrum-Icon", "spectrum-Icon--sizeS")} focusable="false" aria-hidden="true" />
                        </button>
                        <button className={classnames("spectrum-Button", "spectrum-Button--fill", "spectrum-Button--primary", "spectrum-Button--sizeS", "spectrum-Button--iconOnly")}>
                            <DeleteIcon className={classnames("spectrum-Icon", "spectrum-Icon--sizeS")} focusable="false" aria-hidden="true" />
                        </button>
                    </ActionGroup>
                </div>
            </li>
        );
    }

    return (
        <PopOver className={className}>
            <ul>{listItems}</ul>
        </PopOver>
    );
};
