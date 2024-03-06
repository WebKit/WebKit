import { ActionButton } from "../action-group";

import ChevronRight from "./../../assets/Smock_ChevronRight_18_N.svg";
import ChevronLeft from "./../../assets/Smock_ChevronLeft_18_N.svg";
import AddIcon from "./../../assets/Smock_Add_18_N.svg";

const DaysOfTheWeekHeadings = () => {
    const daysOfTheWeek = { Sunday: "S", Monday: "M", Tuesday: "T", Wednesday: "W", Thursday: "T", Friday: "F", Saturday: "S" };

    return Object.entries(daysOfTheWeek).map(([key, value]) =>
        <th key={key} role="columnheader" scope="col" className="spectrum-Calendar-tableCell">
            <abbr className="spectrum-Calendar-dayOfWeek" title={key}>
                {value}
            </abbr>
        </th>
    );
};

const CalendarRow = ({ weekStart }) => {
    const dates = [...Array(7)].map((_, i) => {
        const date = new Date(weekStart);
        date.setDate(weekStart.getDate() + i);
        return date;
    });

    const children = dates.map((date, index) => {
        return (
            <td
                key={index}
                role="gridcell"
                aria-invalid="false"
                className="spectrum-Calendar-tableCell"
                tabIndex="-1"
                aria-disabled="false"
                aria-selected="false"
                title={date.toLocaleDateString("en-US", {
                    weekday: "long",
                    year: "numeric",
                    month: "long",
                    day: "numeric",
                })}
            >
                <span role="presentation" className="spectrum-Calendar-date">
                    {date.getDate()}
                </span>
            </td>
        );
    });

    return <tr role="row">{children}</tr>;
};

const CalendarBody = () => {
    const weekStarts = [new Date(2023, 1, 26), new Date(2023, 2, 5), new Date(2023, 2, 12), new Date(2023, 2, 19), new Date(2023, 2, 26)];
    const children = weekStarts.map((weekStart, index) => <CalendarRow key={index} weekStart={weekStart} />);
    return <tbody role="presentation">{children}</tbody>;
};

const Calendar = () => {
    return (
        <div className="spectrum-Calendar" style={{ width: "280px" }}>
            <div className="spectrum-Calendar-header">
                <div role="heading" aria-live="assertive" aria-atomic="true" className="spectrum-Calendar-title">
                    March 2023
                </div>

                <ActionButton Icon={ChevronLeft} quiet aria-label="Previous" aria-haspopup="false" aria-pressed="false" className="spectrum-ActionButton spectrum-ActionButton--sizeS spectrum-ActionButton--quiet spectrum-Calendar-prevMonth" />
                <ActionButton Icon={ChevronRight} quiet aria-label="Next" aria-haspopup="false" aria-pressed="false" className="spectrum-ActionButton spectrum-ActionButton--sizeS spectrum-ActionButton--quiet spectrum-Calendar-nextMonth" />
            </div>
            <div role="grid" tabIndex="0" aria-readonly="true" className="spectrum-Calendar-body" aria-disabled="false">
                <table role="presentation" className="spectrum-Calendar-table">
                    <thead role="presentation">
                        <tr role="row">{DaysOfTheWeekHeadings()}</tr>
                    </thead>
                    <CalendarBody />
                </table>
            </div>
        </div>
    );
};

export const DatePicker = () => {
    return (
        <div aria-haspopup="dialog" className="spectrum-DatePicker spectrum-ActionGroup-item" aria-disabled="false" aria-readonly="false" aria-required="false">
            <div className="spectrum-Textfield spectrum-Textfield--sizeM spectrum-DatePicker-textfield">
                <input type="text" placeholder="New Sprint" defaultValue="" name="field" autoComplete="" className="spectrum-Textfield-input spectrum-DatePicker-input" />
            </div>

            <button aria-haspopup="listbox" className="spectrum-PickerButton spectrum-PickerButton--icononly spectrum-PickerButton--right spectrum-PickerButton--sizeM spectrum-DatePicker-button">
                <div className="spectrum-PickerButton-fill">
                    <AddIcon className="spectrum-Icon spectrum-Icon--sizeM spectrum-PickerButton-menuIcon" />
                </div>
            </button>

            <div role="presentation" className="spectrum-Popover spectrum-Popover--sizeM spectrum-Popover--bottom date-picker-popover">
                <Calendar />
            </div>
        </div>
    );
};
