# The Daily Broadcast

preview: https://flashdesignory.github.io/news-site-nuxt-static/

> **_NOTE:_** This is not a typical use-case for Nuxt and we encourage developers to follow the [official documentation](https://nuxt.com/docs) for recommended usage of the framework.

This app is a news-site built with [Nuxt](https://nuxt.com/). It utilizes the [News Site Template](https://github.com/flashdesignory/news-site-template) as the basis for styling and functionality.
Since Speedometer expects static files for all apps included, this project's build step uses [static html export](https://content.nuxtjs.org/guide/deploy/static-hosting/).
<br>With this implementation, some features of Nuxt are not available and therefore omitted to ensure compatibility with Speedometer.

## Local Development

Start the local dev server:

```bash
npm run dev
```

Open [http://localhost:3000](http://localhost:3000) with your browser to see the result.

## Deployment of changes

To create new build files, run:

```bash
npm run generate
```

Add, commit and push changes to the working branch.

> **_NOTE:_** `output` folder changed to `docs`, to be able to publish to Github pages.

## Test steps

The Speedometer test consists of navigating between the different pages of the news site.
It includes interactions with the navigation drop-down menu to ensure state changes happen in between the page navigations.
