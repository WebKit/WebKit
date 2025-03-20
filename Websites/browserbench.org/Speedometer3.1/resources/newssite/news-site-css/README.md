# news-site-css

## How to use

This package allows you to use the stylesheets in various ways, either by including the complete rules (index.css, index.min.css) in a link tag, or by importing partial css / css module files in your code.

install the package

```bash
npm install news-site-css
```

including the styles in html with a link tag

```html
<link href="news-site-css/dist/index.min.css" rel="stylesheet" />
```

importing the styles in JavaScript:

```javascript
import "news-site-css/dist/global.css";
```

importing a css module in React:

```javascript
import styles from "news-site-css/dist/footer.module.css";

export default function Footer() {
    return <footer className={styles.footer}></footer>;
}
```
