import { VerticalPopOver } from "./popover.jsx";
import classnames from "classnames";

const ProgressBar = ({ teamNumber, value, index }) => {
    return (
        <div className={classnames("spectrum-ProgressBar", "spectrum-Meter--sizeS", "is-negative", "analytics-bar")} value={value} role="progressbar" data-status={index} aria-valuenow={value} aria-valuemin="0" aria-valuemax="100">
            <div className="spectrum-FieldLabel spectrum-FieldLabel--sizeS spectrum-ProgressBar-label">Team {teamNumber} progress</div>
            <div className="spectrum-FieldLabel spectrum-FieldLabel--sizeS spectrum-ProgressBar-percentage">{value}%</div>
            <div className="spectrum-ProgressBar-track">
                <div className="spectrum-ProgressBar-fill" style={{ width: `${value.toString()}%` }}></div>
            </div>
        </div>
    );
};

export const AnalyticsPopOver = ({ className }) => {
    const children = [];
    const numProgressBars = 5;
    for (let i = 0; i < numProgressBars; i++)
        children.push(<ProgressBar key={i} teamNumber={i} value={(i * 20) % 100} index={i} />);

    return <VerticalPopOver className={className}>{children}</VerticalPopOver>;
};
