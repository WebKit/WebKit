# Testing

Speedometer uses [Selenium](https://www.selenium.dev/) for testing of the application itself.
Tests are located in the `/tests` folder.

[Sinon](https://sinonjs.org/): Standalone test spies, stubs and mocks for JavaScript.
[Mocha](https://mochajs.org/): Testing framework.

## Local Testing

To run this locally you'll need the browsers installed along with the corresponding driver:

-   [chromedriver](https://chromedriver.chromium.org/getting-started)
-   [geckodriver](https://github.com/mozilla/geckodriver/releases)
-   [safaridriver](https://developer.apple.com/documentation/webkit/testing_with_webdriver_in_safari).

Once installed you can run the following scripts:

```bash
npm run test:chrome
npm run test:firefox
npm run test:safari
```

## Automated Testing

Currently Speedometer's tests run automatically, when pushing to the main branch or when opening a pr.
