// Define an universal message passing API.
//
// In particular, this works:
// - cross-origin and
// - cross-browsing-context-group.
//
// It can also be used to receive reports.

const dispatcher_path =
    '/html/cross-origin-opener-policy/resources/dispatcher.py';
const dispatcher_url = new URL(dispatcher_path, location.href).href;

// Return a promise, limiting the number of concurrent accesses to a shared
// resources to |max_concurrent_access|.
const concurrencyLimiter = (max_concurrency) => {
  let pending = 0;
  let waiting = [];
  return async (task) => {
    pending++;
    if (pending > max_concurrency)
      await new Promise(resolve => waiting.push(resolve));
    await task();
    pending--;
    waiting.shift()?.();
  };
}

// The official web-platform-test runner sometimes drop POST requests when
// too many are requested in parallel. Limiting this document to send only one
// at a time fixes the issue.
const sendLimiter = concurrencyLimiter(1);

const send = async function(uuid, message) {
  await sendLimiter(async () => {
    await fetch(dispatcher_url + `?uuid=${uuid}`, {
      method: 'POST',
      body: message
    });
  });
}

const receive = async function(uuid, maybe_timeout) {
  const timeout = maybe_timeout || Infinity;
  let start = performance.now();
  while(performance.now() - start < timeout) {
    let response = await fetch(dispatcher_url + `?uuid=${uuid}`);
    let data = await response.text();
    if (data != 'not ready')
      return data;
    // Save resources & spread the load:
    await new Promise(r => setTimeout(r, 100*Math.random()));
  }
  return "timeout";
}

const receiveReport = async function(uuid, type) {
  while(true) {
    let reports = await receive(uuid);
    if (reports == "timeout")
      return "timeout";
    reports = JSON.parse(reports);

    for(report of reports) {
      if (report?.body?.type == type)
        return report;
    }
  }
}

// Build a set of headers to tests the reporting API. This defines a set of
// matching 'Report-To', 'Cross-Origin-Opener-Policy' and
// 'Cross-Origin-Opener-Policy-Report-Only' headers.
const reportToHeaders = function(uuid) {
  const report_endpoint_url = dispatcher_path + `?uuid=${uuid}`;
  let reportToJSON = {
    'group': `${uuid}`,
    'max_age': 3600,
    'endpoints': [
      {'url': report_endpoint_url.toString()},
    ]
  };
  reportToJSON = JSON.stringify(reportToJSON)
                     .replace(/,/g, '\\,')
                     .replace(/\(/g, '\\\(')
                     .replace(/\)/g, '\\\)=');

  return {
    header: `|header(report-to,${reportToJSON})`,
    coopSameOriginHeader: `|header(Cross-Origin-Opener-Policy,same-origin%3Breport-to="${uuid}")`,
    coopSameOriginAllowPopupsHeader: `|header(Cross-Origin-Opener-Policy,same-origin-allow-popups%3Breport-to="${uuid}")`,
    coopReportOnlySameOriginHeader: `|header(Cross-Origin-Opener-Policy-Report-Only,same-origin%3Breport-to="${uuid}")`,
    coopReportOnlySameOriginAllowPopupsHeader: `|header(Cross-Origin-Opener-Policy-Report-Only,same-origin-allow-popups%3Breport-to="${uuid}")`,
  };
};
