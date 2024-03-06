import classNames from "classnames";
import A11yIcon from "@/assets/a11y-icon";

import styles from "news-site-css/dist/icons-group.module.css";

export default function SettingsIcons({ onClick, id }) {
    return (
        <div className={styles["icons-group"]}>
            <ul className={styles["icons-group-list"]}>
                <li className={styles["icons-group-item"]}>
                    <button onClick={onClick} id={`${id}-a11y`}>
                        <div className={classNames(styles["group-icon"], styles["group-icon-medium"])}>
                            <A11yIcon />
                        </div>
                    </button>
                </li>
            </ul>
        </div>
    );
}
