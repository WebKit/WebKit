# Development Environment

## Prerequisite

Node is required for local development.
We recommend using a Node version manager ([nvm](https://github.com/nvm-sh/nvm) for example) to ensure a stable development environment is used.
Below are the required node / npm versions:

```sh
    "node": ">18.13.0",
    "npm": ">8.19.3"
```

## Installation

In your working directory, open terminal and paste the following commands:

```sh
    git clone https://github.com/WebKit/Speedometer.git
    cd Speedometer
    npm install
```

## Run Development Server

1. In your terminal run:
    ```sh
        npm run dev
    ```
2. Open your browser of choice and navigate to [http://127.0.0.1:7000](http://127.0.0.1:7000)

## Local Server

Speedometer uses [http-server](https://github.com/http-party/http-server), which is a static HTTP server. Meaning it does not provide hot-reloading. By default, the dev script disables caching and local changes can be viewed by simply refreshing your browser window. Additional options of the http-server can be found [here](https://github.com/http-party/http-server#available-options).
