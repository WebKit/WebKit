import classNames from "classnames";
import FacebookIcon from "@/assets/facebook-icon";
import InstagramIcon from "@/assets/instagram-icon";
import TwitterIcon from "@/assets/twitter-icon";

import { useDataContext } from "@/context/data-context";

import styles from "news-site-css/dist/icons-group.module.css";

export default function SocialIcons({ id }) {
    const { links } = useDataContext();

    return (
        <div className={styles["icons-group"]}>
            <ul className={styles["icons-group-list"]}>
                <li className={styles["icons-group-item"]}>
                    <a href={links.social.facebook.href} id={`${id}-facebook`}>
                        <div className={classNames(styles["group-icon"], styles["group-icon-small"])}>
                            <FacebookIcon />
                        </div>
                    </a>
                </li>
                <li className={styles["icons-group-item"]}>
                    <a href={links.social.instagram.href} id={`${id}-instagram`}>
                        <div className={classNames(styles["group-icon"], styles["group-icon-small"])}>
                            <InstagramIcon />
                        </div>
                    </a>
                </li>
                <li className={styles["icons-group-item"]}>
                    <a href={links.social.twitter.href} id={`${id}-twitter`}>
                        <div className={classNames(styles["group-icon"], styles["group-icon-small"])}>
                            <TwitterIcon />
                        </div>
                    </a>
                </li>
            </ul>
        </div>
    );
}
