export default function ArticleHeader({ text, headerClass, link }) {
    if (!text)
        return null;

    return (
        <header className={headerClass}>
            {link
                ? <a href={link}>
                    <h2>{text}</h2>
                </a>
                : <h2>{text}</h2>
            }
        </header>
    );
}
