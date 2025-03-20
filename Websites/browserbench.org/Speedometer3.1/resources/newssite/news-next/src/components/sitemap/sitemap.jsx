import classNames from "classnames";

import { useDataContext } from "@/context/data-context";

import { NavLink } from "react-router-dom";
import { HashLink } from "react-router-hash-link";

import styles from "news-site-css/dist/sitemap.module.css";

export default function Sitemap() {
    const { content } = useDataContext();

    const keys = Object.keys(content);
    const navItems = keys.reduce((result, key) => {
        result.push(key);
        return result;
    }, []);

    return (
        <div className={styles.sitemap}>
            <ul className={styles["sitemap-list"]}>
                {navItems.map((key) =>
                    <li className={styles["sitemap-item"]} key={`sitemap-page-${content[key].name}`}>
                        <NavLink to={content[key].url} className={({ isActive }) => classNames({ [styles.active]: isActive })}>
                            <h4 className={styles["sitemap-header"]}>{content[key].name}</h4>
                        </NavLink>
                        <ul className={styles["sitemap-sublist"]}>
                            {content[key].sections.map((section) =>
                                <li className={styles["sitemap-subitem"]} key={`sitemap-section${section.id}`}>
                                    <HashLink to={`${content[key].url}#${section.id}`}>{section.name}</HashLink>
                                </li>
                            )}
                        </ul>
                    </li>
                )}
            </ul>
        </div>
    );
}
