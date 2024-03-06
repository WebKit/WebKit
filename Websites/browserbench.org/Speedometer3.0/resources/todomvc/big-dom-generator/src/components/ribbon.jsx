import { ActionButton, ActionGroup, ActionItem } from "./action-group";
import { DatePicker } from "./popovers/date-picker";
import { BacklogPopOver } from "./popovers/backlog";
import { AnalyticsPopOver } from "./popovers/analytics";
import { FilterPopOver } from "./popovers/filter";
import { TimelinePopOver } from "./popovers/timeline";
import { ReminderPopOver } from "./popovers/reminder";

import DeleteIcon from "./../assets/Smock_Delete_18_N.svg";
import CutIcon from "./../assets/Smock_Cut_18_N.svg";
import FilterIcon from "./../assets/Smock_Filter_18_N.svg";
import ViewListIcon from "./../assets/Smock_ViewList_18_N.svg";
import GraphTrendIcon from "./../assets/Smock_GraphTrend_18_N.svg";
import GraphGanttIcon from "./../assets/Smock_GraphGantt_18_N.svg";
import ClockIcon from "./../assets/Smock_Clock_18_N.svg";

const Divider = () => {
    return <div className="ribbon-divider spectrum-Divider spectrum-Divider--sizeS spectrum-Divider--vertical" />;
};

export const Ribbon = () => {
    return (
        <div className="ribbon">
            <ActionGroup>
                <ActionItem className="reminder-group">
                    <ActionButton Icon={ClockIcon} label="Send Reminder" quiet />
                    <ReminderPopOver />
                </ActionItem>
                <ActionItem className="backlog-group">
                    <ActionButton Icon={ViewListIcon} label="Backlog" quiet />
                    <BacklogPopOver />
                </ActionItem>
                <ActionItem className="analytics-group">
                    <ActionButton Icon={GraphTrendIcon} label="Analytics" quiet />
                    <AnalyticsPopOver />
                </ActionItem>
            </ActionGroup>
            <Divider />
            <ActionGroup>
                <ActionButton Icon={CutIcon} quiet />
                <ActionButton Icon={DeleteIcon} quiet />
            </ActionGroup>
            <Divider />
            <ActionGroup>
                <ActionItem className="timeline-group">
                    <ActionButton Icon={GraphGanttIcon} label="Timeline" quiet />
                    <TimelinePopOver />
                </ActionItem>
                <ActionItem className="filter-group">
                    <ActionButton Icon={FilterIcon} quiet />
                    <FilterPopOver />
                </ActionItem>
            </ActionGroup>
            <Divider />
            <ActionGroup>
                <DatePicker />
            </ActionGroup>
        </div>
    );
};
