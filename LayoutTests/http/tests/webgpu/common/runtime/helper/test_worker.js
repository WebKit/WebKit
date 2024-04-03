/**
* AUTO-GENERATED - DO NOT EDIT. Source: https://github.com/gpuweb/cts
**/import { LogMessageWithStack } from '../../internal/logging/log_message.js';

import { timeout } from '../../util/timeout.js';
import { assert } from '../../util/util.js';

import { kDefaultCTSOptions } from './options.js';


/** Query all currently-registered service workers, and unregister them. */
function unregisterAllServiceWorkers() {
  void navigator.serviceWorker.getRegistrations().then((registrations) => {
    for (const registration of registrations) {
      void registration.unregister();
    }
  });
}

// NOTE: This code runs on startup for any runtime with worker support. Here, we use that chance to
// delete any leaked service workers, and register to clean up after ourselves at shutdown.
unregisterAllServiceWorkers();
window.addEventListener('beforeunload', () => {
  unregisterAllServiceWorkers();
});

class TestBaseWorker {

  resolvers = new Map();

  constructor(worker, ctsOptions) {
    this.ctsOptions = { ...(ctsOptions || kDefaultCTSOptions), ...{ worker } };
  }

  onmessage(ev) {
    const query = ev.data.query;
    const result = ev.data.result;
    if (result.logs) {
      for (const l of result.logs) {
        Object.setPrototypeOf(l, LogMessageWithStack.prototype);
      }
    }
    this.resolvers.get(query)(result);
    this.resolvers.delete(query);

    // MAINTENANCE_TODO(kainino0x): update the Logger with this result (or don't have a logger and
    // update the entire results JSON somehow at some point).
  }

  async makeRequestAndRecordResult(
  target,
  rec,
  query,
  expectations)
  {
    const request = {
      query,
      expectations,
      ctsOptions: this.ctsOptions
    };
    target.postMessage(request);

    const workerResult = await new Promise((resolve) => {
      assert(!this.resolvers.has(query), "can't request same query twice simultaneously");
      this.resolvers.set(query, resolve);
    });
    rec.injectResult(workerResult);
  }
}

export class TestDedicatedWorker extends TestBaseWorker {


  constructor(ctsOptions) {
    super('dedicated', ctsOptions);
    const selfPath = import.meta.url;
    const selfPathDir = selfPath.substring(0, selfPath.lastIndexOf('/'));
    const workerPath = selfPathDir + '/test_worker-worker.js';
    this.worker = new Worker(workerPath, { type: 'module' });
    this.worker.onmessage = (ev) => this.onmessage(ev);
  }

  async run(
  rec,
  query,
  expectations = [])
  {
    await this.makeRequestAndRecordResult(this.worker, rec, query, expectations);
  }
}

export class TestWorker extends TestDedicatedWorker {}

export class TestSharedWorker extends TestBaseWorker {


  constructor(ctsOptions) {
    super('shared', ctsOptions);
    const selfPath = import.meta.url;
    const selfPathDir = selfPath.substring(0, selfPath.lastIndexOf('/'));
    const workerPath = selfPathDir + '/test_worker-worker.js';
    const worker = new SharedWorker(workerPath, { type: 'module' });
    this.port = worker.port;
    this.port.start();
    this.port.onmessage = (ev) => this.onmessage(ev);
  }

  async run(
  rec,
  query,
  expectations = [])
  {
    await this.makeRequestAndRecordResult(this.port, rec, query, expectations);
  }
}

export class TestServiceWorker extends TestBaseWorker {
  constructor(ctsOptions) {
    super('service', ctsOptions);
  }

  async run(
  rec,
  query,
  expectations = [])
  {
    const [suite, name] = query.split(':', 2);
    const fileName = name.split(',').join('/');
    const serviceWorkerURL = new URL(
      `/out/${suite}/webworker/${fileName}.worker.js`,
      window.location.href
    ).toString();

    // If a registration already exists for this path, it will be ignored.
    const registration = await navigator.serviceWorker.register(serviceWorkerURL, {
      type: 'module'
    });
    // Make sure the registration we just requested is active. (We don't worry about it being
    // outdated from a previous page load, because we wipe all service workers on shutdown/startup.)
    while (!registration.active || registration.active.scriptURL !== serviceWorkerURL) {
      await new Promise((resolve) => timeout(resolve, 0));
    }
    const serviceWorker = registration.active;

    navigator.serviceWorker.onmessage = (ev) => this.onmessage(ev);
    await this.makeRequestAndRecordResult(serviceWorker, rec, query, expectations);
  }
}