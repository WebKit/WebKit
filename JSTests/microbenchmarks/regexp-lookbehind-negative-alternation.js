let agents = [
    "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/120.0.0.0 Safari/537.36",
    "Mozilla/5.0 (compatible; Googlebot/2.1; +http://www.google.com/bot.html)",
    "Mozilla/5.0 (Macintosh; Intel Mac OS X 14_2) AppleWebKit/605.1.15 (KHTML, like Gecko) Version/17.2 Safari/605.1.15",
    "curl/8.4.0",
    "Mozilla/5.0 (compatible; YandexBot/3.0; +http://yandex.com/bots) yasearch",
    "libhttp/1.0 a cubot",
];
let re = /(?<! cu)bot|(?<!lib)http|(?<! ya(?:yandex)?)search|crawler|spider|curl/i;
let result = 0;
for (let i = 0; i < 3e4; ++i) {
    for (let agent of agents)
        result += re.test(agent);
}
if (result !== 3 * 3e4)
    throw new Error("Bad result: " + result);
