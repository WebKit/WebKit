# The Daily Broadcast

> **_NOTE:_** This is not a typical use-case for Next.js and we encourage developers to follow the [official documentation](https://vercel.com/docs) for recommended usage of the framework.

This app is a news-site built with [Next.js](https://nextjs.org/). It utilizes the [News Site Template](https://github.com/flashdesignory/news-site-template) as the basis for styling and functionality.
Since Speedometer expects static files for all apps included, this project's build step uses [static html export](https://nextjs.org/docs/pages/building-your-application/deploying/static-exports).
<br>With this implementation, some features of Next.js are not available and therefore omitted to ensure compatibility with Speedometer.

## Local Development

Start the local dev server:

```bash
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) with your browser to see the result.

## Deployment of changes

To ensure Next.js lint rules are followed, run:

```bash
npm run lint
```

To create new build files, run:

```bash
npm run build
```

Add, commit and push changes to the working branch.

## Test steps

The Speedometer test consists of navigating between the different pages of the news site.
It includes interactions with the navigation drop-down menu to ensure state changes happen in between the page navigations.
