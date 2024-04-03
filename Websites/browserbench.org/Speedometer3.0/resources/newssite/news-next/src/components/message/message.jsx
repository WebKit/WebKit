import classNames from "classnames";

import styles from "news-site-css/dist/message.module.css";

export function Message({ message, onClose }) {
    if (!message)
        return null;

    const { title, description } = message;
    return (
        <div className={classNames(styles.message, styles.open)}>
            <button id="close-message-link" className={styles["message-close-button"]} onClick={onClose} title="Close Button">
                <div className={classNames(styles["message-close-button-icon"], "animated-icon", "close-icon", "hover")} title="Close Icon">
                    <span className="animated-icon-inner">
                        <span></span>
                        <span></span>
                    </span>
                </div>
            </button>
            {title
                ? <header className={styles["message-header"]}>
                    <h2>{title}</h2>
                </header>
                : null}
            <section className={styles["message-body"]}>
                <div className={styles["message-description"]}>{description}</div>
            </section>
        </div>
    );
}
