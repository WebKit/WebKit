# WKR

New WKR bot implementation for WebKit Slack #changes channel.
This bot is fetching git.webkit.org RSS feed periodically, extracting data from that, and posting them to #changes channel to replace IRC's WKR bot purpose.

## Steps to run

1. Run `npm install` to install libraries
2. Put `.env` file, which includes `slackURL="<Slack Endpoint URL>"` (See [https://webkit.slack.com/apps/A0F7XDUAZ-incoming-webhooks?next_id=0](this page) for getting this URL).
3. Run `npm start`

## Details

The lastest posted revision data is stored in `data/` directory. You can clean up state if you remove files in `data/`.
