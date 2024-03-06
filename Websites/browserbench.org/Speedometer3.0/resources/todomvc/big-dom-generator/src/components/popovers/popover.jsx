import { Children } from "react";
import classnames from "classnames";

export const PopOver = ({ children, className, startRight }) => {
    const popOverClassName = classnames("spectrum-Popover", { "spectrum-Popover--bottom": !startRight }, { "spectrum-Popover--bottom-right": startRight }, className);
    return (
        <div className={popOverClassName} style={{ marginTop: "25px", padding: "5px" }}>
            {children}
        </div>
    );
};

export const VerticalPopOver = ({ children, className, startRight }) => {
    const actionItems = Children.toArray(children).map((child, index) =>
        <div key={index} className="spectrum-ActionGroup-item">
            {child}
        </div>
    );
    return (
        <PopOver className={className} startRight={startRight}>
            <div className="spectrum-ActionGroup spectrum-ActionGroup--vertical spectrum-ActionGroup--sizeS">{actionItems}</div>
        </PopOver>
    );
};

export const OptionsPopOver = ({ numOptions, className, startRight }) => {
    const options = [];
    for (let i = 0; i < numOptions; i++) {
        options.push(
            <li key={i} className="spectrum-Menu-item" role="menuitem" tabIndex="0">
                <span className="spectrum-Menu-itemLabel">Hidden Option {i}</span>
            </li>
        );
    }
    const classNamePopOver = classnames("spectrum-Popover", { "spectrum-Popover--bottom": !startRight }, { "spectrum-Popover--bottom-right": startRight }, className);
    return (
        <div className={classNamePopOver} style={{ marginTop: "25px", padding: "5px" }}>
            <ul className="spectrum-Menu" role="menu">
                {options}
            </ul>
        </div>
    );
};
