import classnames from "classnames";

export const ActionItem = ({ className, children }) => {
    return <div className={classnames("spectrum-ActionGroup-item", className)}>{children}</div>;
};

export const ActionButton = ({ Icon, label, quiet, className, ...rest }) => {
    const buttonClassName = classnames("spectrum-ActionButton", "spectrum-ActionButton--sizeM", { "spectrum-ActionButton--quiet": quiet }, className);
    const text = label ? <span className="spectrum-ActionButton-label">{label}</span> : null;
    return (
        <button className={buttonClassName} {...rest}>
            {Icon && <Icon className="spectrum-Icon spectrum-Icon--sizeM spectrum-ActionButton-icon" focusable="false" aria-hidden="true" />}
            {text}
        </button>
    );
};

export const ActionGroup = ({ children }) => {
    return <div className="spectrum-ActionGroup spectrum-ActionGroup--compact spectrum-ActionGroup--sizeM">{children}</div>;
};

export const ActionGroupVertical = ({ children }) => {
    return <div className="spectrum-ActionGroup spectrum-ActionGroup--vertical spectrum-ActionGroup--sizeS">{children}</div>;
};
