import { VerticalPopOver } from "./popover";
import { ActionButton } from "./../action-group";
import classnames from "classnames";

import ChevronUpIcon from "./../../assets/Smock_ChevronUp_18_N.svg";
import ChevronDownIcon from "./../../assets/Smock_ChevronDown_18_N.svg";

const Stepper = () => {
    return (
        <div className="spectrum-Stepper">
            <label htmlFor="stepper-m" className="spectrum-FieldLabel spectrum-FieldLabel--sizeS">
                Sprints
            </label>
            <div className="spectrum-Textfield spectrum-Textfield--sizeM spectrum-Stepper-textfield">
                <input type="text" placeholder="1" autoComplete="" className="spectrum-Textfield-input spectrum-Stepper-input" id="stepper-m" />
            </div>

            <span className="spectrum-Stepper-buttons">
                <ActionButton Icon={ChevronUpIcon} aria-haspopup="false" aria-pressed="false" className="spectrum-Stepper-stepUp" />
                <ActionButton Icon={ChevronDownIcon} aria-haspopup="false" aria-pressed="false" className="spectrum-Stepper-stepDown" />
            </span>
        </div>
    );
};

const TagGroup = () => {
    const tags = [
        { label: "Tag 1", className: "spectrum-Tag--sizeS" },
        { label: "Tag 2", className: "spectrum-Tag--sizeS is-invalid" },
        { label: "Tag 3", className: "spectrum-Tag--sizeS is-disabled" },
    ];

    return (
        <div className="spectrum-TagGroup" role="list" aria-label="list">
            {tags.map((tag, index) =>
                <div className={`spectrum-Tag spectrum-TagGroup-item ${tag.className}`} role="listitem" key={index}>
                    <span className="spectrum-Tag-label">{tag.label}</span>
                </div>
            )}
        </div>
    );
};

export const FilterPopOver = ({ className }) => {
    const popOverClassName = classnames("filter-pop-over", className);
    return (
        <VerticalPopOver className={popOverClassName}>
            <div className="spectrum-Textfield">
                <label htmlFor="textfield-1" className="spectrum-FieldLabel spectrum-FieldLabel--sizeS">
                    Name
                </label>
                <input id="textfield-1" type="text" name="field" defaultValue="Sprint one" className="spectrum-Textfield-input filter-input" pattern="[\w\s]+" aria-describedby="character-count-6" />
            </div>
            <Stepper />
            <TagGroup />
            <div className="spectrum-Switch spectrum-Switch--sizeS">
                <input type="checkbox" className="spectrum-Switch-input" id="switch-onoff-1" defaultChecked={true} />
                <span className="spectrum-Switch-switch"></span>
                <label className="spectrum-Switch-label" htmlFor="switch-onoff-1">
                    Completed Sprints
                </label>
            </div>
        </VerticalPopOver>
    );
};
