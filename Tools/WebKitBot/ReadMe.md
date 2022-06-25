# WebKitBot

New WebKitBot implementation for WebKit Slack #changes and #dev channels.
This node.js app has two bot features, WKR and WebKitBot.
WKR is fetching git.webkit.org RSS feed periodically, extracting data from that, and posting them to #changes channel.
WebKitBot is an interactive bot in #dev to serve `revert` feature for you without clobbering your WebKit working copy.

## Steps to run

1. Run `npm install` to install libraries
2. Put `.env` file, which should include five parameters. See "Configuration" section below.
3. Run `npm start`

## Configuration

Put `.env` file including the followings.

```
slackURL="<Slack Endpoint URL>"
SLACK_TOKEN="<Slack API Token>"
webkitWorkingDirectory="/path/to/WebKit/repository/used/used/by/revert/command"
webkitBugzillaUsername="commit-queue@webkit.org"
webkitBugzillaPassword="<commit-queue's password>"
```

- Find Slack endpoint URL from [https://webkit.slack.com/apps/A0F7XDUAZ-incoming-webhooks?next_id=0](Incomming WebHook page).
- Find Slack API Token (called `Bot User OAuth Access Token`) from [https://api.slack.com/apps/A013CJ77QFM](WebKitBot's app page).

## How to test

- Test via `npm test`.
- Lint via `npm run lint`.

## Details

The lastest posted revision data is stored in `data/` directory. You can clean up state if you remove files in `data/`.
