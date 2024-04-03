import { useState, useEffect } from "react";

import styles from "news-site-css/dist/toggle.module.css";

export default function Toggle({ id, label, onChange, checked }) {
    const [isSelected, setIsSelected] = useState(false);

    useEffect(() => {
        setIsSelected(checked);
    }, [checked]);

    function handleChange(e) {
        setIsSelected(e.target.checked);
        onChange(e);
    }

    return (
        <div className={styles["toggle-outer"]}>
            <div className={styles["toggle-description"]}>{label}</div>
            <div className={styles["toggle-container"]}>
                <label className={styles.label} htmlFor={`${id}-toggle`}>
                    <input type="checkbox" id={`${id}-toggle`} checked={isSelected} onChange={handleChange} />
                    <span className={styles.switch}></span>
                    <div className="visually-hidden">selected: {isSelected ? "true" : "false"}</div>
                </label>
            </div>
        </div>
    );
}
