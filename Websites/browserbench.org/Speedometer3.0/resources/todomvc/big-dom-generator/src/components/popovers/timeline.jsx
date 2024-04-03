import classnames from "classnames";

import AlertIcon from "./../../assets/Smock_Alert_18_N.svg";

const TimelineComponent = (status) => {
    const classNames = classnames("spectrum-Steplist", status);
    return (
        <div className={classNames} role="list">
            <div class={classnames("spectrum-Steplist-item", "is-complete")} role="listitem" aria-posinset="1" aria-setsize="4">
                <span class="spectrum-Steplist-label">Step 1</span>
                <span class="spectrum-Steplist-markerContainer">
                    <span class="spectrum-Steplist-marker"></span>
                </span>
                <span class="spectrum-Steplist-segment"></span>
            </div>
            <div class={classnames("spectrum-Steplist-item", "is-complete")} role="listitem" aria-posinset="2" aria-setsize="4">
                <span class="spectrum-Steplist-label">Step 2</span>
                <span class="spectrum-Steplist-markerContainer">
                    <span class="spectrum-Steplist-marker"></span>
                </span>
                <span class="spectrum-Steplist-segment"></span>
            </div>
            <div class={classnames("spectrum-Steplist-item", "is-selected")} role="listitem" aria-posinset="3" aria-setsize="4" aria-current="step">
                <span class="spectrum-Steplist-label">Step 3</span>
                <span class="spectrum-Steplist-markerContainer">
                    <span class="spectrum-Steplist-marker"></span>
                </span>
                <span class="spectrum-Steplist-segment"></span>
            </div>
            <div class="spectrum-Steplist-item" role="listitem" aria-posinset="4" aria-setsize="4">
                <span class="spectrum-Steplist-label">Step 4</span>
                <span class="spectrum-Steplist-markerContainer">
                    <span class="spectrum-Steplist-marker"></span>
                </span>
                <span class="spectrum-Steplist-segment"></span>
            </div>
        </div>
    );
};

export const TimelinePopOver = ({ className }) => {
    const popOverClassName = classnames("spectrum-Popover", "spectrum-Popover--bottom-right", className);
    const inputValues = ["", "1", "0", "", "1"];
    const popOverChildren = inputValues.map((inputValue, i) => {
        return (
            <li key={i}>
                <input type="text" name="field" value={inputValue} placeholder="Owner ID" />
                <TimelineComponent />
                <div>
                    <button className={classnames("spectrum-Button", "spectrum-Button--fill", "spectrum-Button--accent", "spectrum-Button--sizeS")}>
                        <span className="spectrum-Button-label">Status</span>
                    </button>
                    <AlertIcon />
                </div>
            </li>
        );
    });
    return (
        <div class={popOverClassName}>
            <ul>{popOverChildren}</ul>
        </div>
    );
};
