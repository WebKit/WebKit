import classNames from "classnames";
import toastStyles from "news-site-css/dist/toast.module.css";
import buttonStyles from "news-site-css/dist/button.module.css";

export default function Toast({ onClose, notification, onAccept, onReject }) {
    const { title, description, actions } = notification;
    return (
        <div className={classNames(toastStyles.toast, toastStyles.open)}>
            <button id="close-toast-link" className={toastStyles["toast-close-button"]} onClick={onClose} title="Close Button">
                <div className={classNames(toastStyles["toast-close-button-icon"], "animated-icon", "close-icon", "hover")} title="Close Icon">
                    <span className="animated-icon-inner">
                        <span></span>
                        <span></span>
                    </span>
                </div>
            </button>
            {title
                ? <header className={toastStyles["toast-header"]}>
                    <h2>{title}</h2>
                </header>
                : null}
            <section className={toastStyles["toast-body"]}>
                <div className={toastStyles["toast-description"]}>{description}</div>
                <div className={toastStyles["toast-actions"]}>
                    {actions.map((action) => {
                        const id = `toast-${action.type}-button`;
                        return (
                            <button key={id} id={id} className={classNames(buttonStyles.button, buttonStyles[`${action.priority}-button`], toastStyles["toast-actions-button"])} onClick={action.type === "accept" ? onAccept : onReject}>
                                {action.name}
                            </button>
                        );
                    })}
                </div>
            </section>
        </div>
    );
}
