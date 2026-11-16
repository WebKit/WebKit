import classnames from "classnames";

import CheckmarkIcon from "./../../assets/Smock_Checkmark_18_N.svg";
import CheckmarkIcon75 from "./../../assets/CheckmarkSize75.svg";

export const ReminderPopOver = ({ className }) => {
    let children = [];
    children = children.concat([
        <label className={classnames("spectrum-Checkbox", "spectrum-Checkbox--sizeM", "high-priority")}>
            <input type="checkbox" className="spectrum-Checkbox-input" title="Urgent" />
            <span className="spectrum-Checkbox-box">
                <CheckmarkIcon className="spectrum-Icon spectrum-UIIcon-Checkmark100 spectrum-Checkbox-checkmark" focusable="false" aria-hidden="true" />
            </span>
            <label className="spectrum-Checkbox-label">Urgent</label>
        </label>,
        <div className={classnames("spectrum-Textfield", "spectrum-Textfield--sizeS", "spectrum-Textfield--sideLabel", "is-invalid")}>
            <label for="textfield-sidelabel" className={classnames("spectrum-FieldLabel", "spectrum-FieldLabel--sizeM")}>
                Task ID
            </label>
            <CheckmarkIcon75 className={classnames("spectrum-Icon", "spectrum-UIIcon-Checkmark75", "spectrum-Textfield-validationIcon")} focusable="false" aria-hidden="true" />
            <input id="textfield-s-valid" type="text" name="field" placeholder="Task ID" class="spectrum-Textfield-input" pattern="[\w\s]+" required />
        </div>,
        <div className={classnames("spectrum-Textfield", "spectrum-Textfield--sizeS", "spectrum-Textfield--sideLabel", "is-invalid")}>
            <label for="textfield-sidelabel" className={classnames("spectrum-FieldLabel", "spectrum-FieldLabel--sizeM")}>
                Email
            </label>
            <input id="textfield-s-valid" type="text" name="field" placeholder="Email" class="spectrum-Textfield-input" pattern="[\w\s]+" required />
        </div>,

        <div className={classnames("spectrum-Textfield", "spectrum-Textfield--sizeS", "spectrum-Textfield--sideLabel", "is-invalid")}>
            <label for="textfield-sidelabel" className={classnames("spectrum-FieldLabel", "spectrum-FieldLabel--sizeM")}>
                Phone
            </label>
            <input id="textfield-s-valid" type="text" name="field" placeholder="Phone" class="spectrum-Textfield-input" pattern="[\w\s]+" required />
        </div>,
        <div>
            <button class="spectrum-Button spectrum-Button--fill spectrum-Button--primary spectrum-Button--sizeS">
                <span class="spectrum-Button-label">Send</span>
            </button>
        </div>,
    ]);
    let popOverClassName = classnames("spectrum-Popover", "spectrum-Popover--bottom", className);

    return <div className={popOverClassName}>{children}</div>;
};
