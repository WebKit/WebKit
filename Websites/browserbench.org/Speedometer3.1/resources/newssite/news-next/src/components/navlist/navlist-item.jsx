import { NavLink } from "react-router-dom";
import classNames from "classnames";

import styles from "news-site-css/dist/navbar.module.css";

export default function NavListItem({ id, label, url, callback, itemClass }) {
    return (
        <li className={classNames(styles["navbar-item"], itemClass)} onClick={callback}>
            <NavLink to={url} id={id} className={({ isActive }) => classNames({ [styles.active]: isActive })}>
                {label}
            </NavLink>
        </li>
    );
}
