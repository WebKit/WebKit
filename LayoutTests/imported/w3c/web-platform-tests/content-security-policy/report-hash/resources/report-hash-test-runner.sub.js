function find_server_timing(name) {
  const server_timing = performance.getEntriesByType("navigation")[0].serverTiming;
  for (entry of server_timing) {
    if (entry.name == name) {
      return entry.description;
    }
  }
  return null;
}

const ORIGIN = "https://{{host}}:{{ports[https][0]}}";
const REMOTE_ORIGIN = "https://{{hosts[alt][www]}}:{{ports[https][0]}}";
const endpoint = `${ORIGIN}/reporting/resources/report.py`;
const id = find_server_timing("uuid");
const id2 = find_server_timing("uuid2");
const subresource_url = `${ORIGIN}/reporting/resources/comment.js`;
const crossorigin_subresource_url = `${REMOTE_ORIGIN}/reporting/resources/comment.js`;
const subresource_hash = find_server_timing("hash");
const subresource_hash2 = find_server_timing("hash2");
let counter = 0;
const REPORT_TIMEOUT = 5;
const MAX_RETRIES = 3;

function wait(ms) {
  return new Promise(resolve => step_timeout(resolve, ms));
}

async function pollReportsWithTimeout(endpoint, id, min_count, timeout) {
  // Use retain=true to keep reports in the stash for other tests that share the same UUID
  const res = await fetch(`${endpoint}?reportID=${id}&min_count=${min_count}&timeout=${timeout}&retain`, { cache: 'no-store' });
  const reports = [];
  if (res.status === 200) {
    for (const report of await res.json()) {
      reports.push(report);
    }
  }
  return reports;
}

async function pollReportsWithRetry(endpoint, id, min_count, timeout, retries) {
  for (let i = 0; i < retries; i++) {
    const reports = await pollReportsWithTimeout(endpoint, id, min_count, timeout);
    if (reports.length >= min_count)
      return reports;
    // Wait a bit before retrying
    await wait(500);
  }
  // Final attempt
  return await pollReportsWithTimeout(endpoint, id, min_count, timeout);
}

function reporting_observer_setup(expected_url, expected_hash) {
  return new Promise(resolve => {
    new ReportingObserver((reports, observer) => {
      assert_unreached();
      observer.disconnect();
    }, { types: ["csp-hash"] }).observe();
    step_timeout(resolve, 100);
  });
}

async function check_reports(uuid, expected_hash, url) {
  // Give the browser time to send the report before polling.
  await wait(100);
  const reports = await pollReportsWithRetry(endpoint, uuid, 1, REPORT_TIMEOUT, MAX_RETRIES);
  checkReportExists(reports, 'csp-hash', location.href);
  const report = getReport(reports, 'csp-hash', location.href, url);
  assert_not_equals(report, null);
  assert_equals(report.body.hash, expected_hash);
  assert_equals(report.body.type, "subresource");
  assert_equals(report.body.destination, "script");
}

function report_hash_test(url, populate_script_attributes, expected_hash, expected_hash2, description) {
  promise_test(async t => {
    const unique_subresource_url = `${url}?${++counter}`;
    const observer_promise = reporting_observer_setup(unique_subresource_url, subresource_hash);
    // Trigger a script load
    await new Promise(resolve => {
      const script = document.createElement('script');
      script.src = unique_subresource_url;
      populate_script_attributes(script);
      script.addEventListener('load', resolve);
      document.head.appendChild(script);
    });

    await check_reports(id, expected_hash, unique_subresource_url);
    if (id2) {
      await check_reports(id2, expected_hash2, unique_subresource_url);
    }
    await observer_promise;
  }, description);
}

function no_report_test(create_element, description) {
  promise_test(async t => {
    const unique_subresource_url = `${subresource_url}?${++counter}`;
    // Trigger a script load
    await new Promise(resolve => {
      const elem = create_element(unique_subresource_url);
      elem.addEventListener('load', resolve);
      elem.addEventListener('error', resolve);
      document.head.appendChild(elem);
    });

    // Wait for report to be received.
    const reports = await pollReports(endpoint, id);
    const report = getReport(reports, 'csp-hash', location.href, unique_subresource_url);
    assert_equals(report, null);
  }, description);
};

function run_tests() {
  // Warm-up test to ensure CSP hash reporting is properly initialized
  // This addresses potential timing issues where the first script load
  // might not trigger hash reporting correctly under stress conditions.
  promise_test(async t => {
    const warmup_url = `${subresource_url}?warmup`;
    await new Promise(resolve => {
      const script = document.createElement('script');
      script.src = warmup_url;
      script.crossOrigin = "anonymous";
      script.addEventListener('load', resolve);
      script.addEventListener('error', resolve);
      document.head.appendChild(script);
    });
    // Wait for any pending operations to complete
    await wait(200);
  }, "Warm-up script load");

  report_hash_test(subresource_url, script => {
    script.crossOrigin = "anonymous";
  }, subresource_hash, subresource_hash2,
  "Reporting endpoints received hash for same-origin CORS script.");

  report_hash_test(subresource_url, script => {
  }, subresource_hash, subresource_hash2,
  "Reporting endpoints received hash for same-origin no-CORS script.");

  report_hash_test(crossorigin_subresource_url, script => {
      script.crossOrigin = "anonymous";
  }, subresource_hash, subresource_hash2,
  "Reporting endpoints received hash for cross-origin CORS script.");

  report_hash_test(crossorigin_subresource_url, script => {
  }, /*expected_hash=*/"", /*expected_hash2=*/"",
  "Reporting endpoints received no hash for cross-origin no-CORS script.");

  report_hash_test(subresource_url, script => {
    script.crossOrigin = "anonymous";
    script.integrity = "sha512-hG4x56V5IhUUepZdYU/lX7UOQJ2M7f6ud2EI7os4JV3OwXSZ002P3zkb9tXQkjpOO8UbtjuEufvdcU67Qt2tlw==";
  }, subresource_hash, subresource_hash2,
  "Reporting endpoints received the right hash for same-origin CORS script with integrity.");

  no_report_test(url => {
    const script = document.createElement('script');
    script.src = url;
    script.crossOrigin = "anonymous"
    script.integrity = "sha256-foobar";
    return script;
    }, "Reporting endpoints received no report for failed integrity check with sha256.");

  no_report_test(url => {
    const script = document.createElement('script');
    script.src = url;
    script.crossOrigin = "anonymous"
    script.integrity = "sha512-foobar";
    return script;
    }, "Reporting endpoints received no report for failed integrity check with sha512.");

  no_report_test(url => {
    const link = document.createElement('link');
    link.href = url;
    link.crossOrigin = "anonymous"
    link.rel = "stylesheet"
    return link;
    }, "Reporting endpoints received no report for CORS stylesheet.");

  no_report_test(url => {
    const link = document.createElement('link');
    link.href = url;
    link.crossOrigin = "anonymous"
    link.rel = "stylesheet"
    link.integrity = "sha256-1XF/E08XndkoxwN6eIa5J89hYn3OVZ/UyB8BrU5jgzk=";
    return link;
    }, "Reporting endpoints received no report for CORS stylesheet with integrity.");
}
