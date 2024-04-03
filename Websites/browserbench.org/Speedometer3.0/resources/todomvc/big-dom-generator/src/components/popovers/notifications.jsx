import classnames from "classnames";

import CheckmarkIcon from "./../../assets/Smock_Checkmark_18_N.svg";

export const NotificationsPopOver = () => {
    const notifications = [
        {
            title: "Notification 1",
            age: "1 day ago",
            checked: false,
            priority: "0",
        },
        {
            title: "Notification 2",
            age: "2 days ago",
            checked: false,
            priority: "1",
        },
        {
            title: "Notification 3",
            age: "2 days ago",
            checked: true,
            priority: "2",
        },
        {
            title: "Notification 4",
            age: "3 days ago",
            checked: false,
            priority: "1",
        },
        {
            title: "Notification 5",
            age: "10 days ago",
            checked: false,
            priority: "0",
        },
    ];

    const listItems = notifications.map((notification, index) =>
        <li className={classnames({ completed: notification.checked })} data-priority={notification.priority} key={index}>
            <div className={classnames("spectrum-Checkbox", "spectrum-Checkbox--sizeS")}>
                <input type="checkbox" className="spectrum-Checkbox-input" id={`checkbox-${index}`} defaultChecked={notification.checked} />
                <div className="spectrum-Checkbox-box">
                    <CheckmarkIcon className={classnames("spectrum-Icon", "spectrum-UIIcon-Checkmark100", "spectrum-Checkbox-checkmark")} />
                </div>
                <label className="spectrum-Checkbox-label">{notification.title}</label>
                <label className="spectrum-Checkbox-label">{notification.age}</label>
            </div>
        </li>
    );

    return (
        <div className={classnames("spectrum-Popover", "spectrum-Popover--bottom-right")}>
            <div className={classnames("spectrum-FieldGroup", "spectrum-FieldGroup--toplabel", "spectrum-FieldGroup--vertical", "spectrum-Popover--bottom-right is-open")} role="group" aria-labelledby="checkboxgroup-label-1">
                <div className="notification-tabs">
                    <div className={classnames("spectrum-FieldLabel", "spectrum-FieldLabel--sizeM")} id="checkboxgroup-label-1">
                        Messages
                    </div>
                    <div className={classnames("spectrum-FieldLabel", "spectrum-FieldLabel--sizeM", "is-active")} id="checkboxgroup-label-2">
                        Notifications
                    </div>
                </div>

                <button>Mark All as Read</button>

                <ul className="notifications-list" role="list">
                    {listItems}
                </ul>
            </div>
        </div>
    );
};
