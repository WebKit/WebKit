import Article from "@/components/article/article";

import styles from "news-site-css/dist/layout.module.css";

export default function Section({ section }) {
    return (
        <>
            {section.name
                ? <div id={section.id} className={styles["row-header"]}>
                    <h2>{section.name}</h2>
                </div>
                : null}
            <section className={styles.row}>
                {section.articles.map((article, index) =>
                    <Article key={`${section.id}-${index}`} article={article} />
                )}
            </section>
        </>
    );
}
