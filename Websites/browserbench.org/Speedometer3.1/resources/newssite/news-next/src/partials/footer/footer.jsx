import { useState } from "react";
import { createPortal } from "react-dom";
import Dialog from "@/components/dialog/dialog";
import SettingsIcons from "../icons/settings-icons";
import SocialIcons from "../icons/social-icons";
import Sitemap from "@/components/sitemap/sitemap";
import { useDataContext } from "@/context/data-context";

import styles from "news-site-css/dist/footer.module.css";

export default function Footer() {
    const [showPortal, setShowPortal] = useState(false);
    const { footer, links } = useDataContext();

    function openPortal() {
        setShowPortal(true);
    }

    function closePortal() {
        setShowPortal(false);
    }

    return (
        <>
            <footer className={styles["page-footer"]}>
                <div className={styles["footer-row"]}>
                    <div className={styles["footer-column-center"]}>
                        <Sitemap />
                    </div>
                </div>
                <div className={styles["footer-row"]}>
                    <div className={styles["footer-column-center"]}>
                        <div className={styles["footer-links"]}>
                            <ul className={styles["footer-links-list"]}>
                                {Object.keys(links.legal).map((key) => {
                                    const item = links.legal[key];
                                    return (
                                        <li className={styles["footer-links-item"]} key={`footer-links-item-${key}`}>
                                            <a href={item.href} id={`footer-link-${key}`} className={styles["footer-link"]}>
                                                {item.label}
                                            </a>
                                        </li>
                                    );
                                })}
                            </ul>
                        </div>
                    </div>
                </div>
                <div className={styles["footer-row"]}>
                    <div className={styles["footer-column-left"]}>
                        <SocialIcons id="footer-social-icons" />
                    </div>
                    <div className={styles["footer-column-center"]}>
                        © {new Date().getFullYear()} {footer.copyright.label}
                    </div>
                    <div className={styles["footer-column-right"]}>
                        <SettingsIcons onClick={openPortal} id="footer-settings-icons" />
                    </div>
                </div>
            </footer>
            {showPortal ? createPortal(<Dialog onClose={closePortal} />, document.getElementById("settings-container")) : null}
        </>
    );
}
