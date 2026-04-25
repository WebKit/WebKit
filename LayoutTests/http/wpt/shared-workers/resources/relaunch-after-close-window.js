let sharedWorker
let count = 0
const messageLogger = e => { log(`    Received message from worker: '${e.data}'`) }

function testComplete(result) {
  opener.dispatchEvent(new MessageEvent("result", { data: result }))
}

// TESTS
async function beginTestForPage1() {
  if (!await initSharedWorker('resources/relaunch-after-close-sharedworker.js')) { return }

  // Prepare for when Page 2 will shutdown the SharedWorker
  const shutdownListener = e => {
    if (e.data === 'shutdown') {
      sharedWorker.port.removeEventListener('message', shutdownListener)
      disonnectFromSharedWorker()
    }
  }

  sharedWorker.port.addEventListener('message', shutdownListener)
}

async function beginTestForPage2() {
  if (!await initSharedWorker('relaunch-after-close-sharedworker.js')) { return }

  while (count < 5) {
    if (!await tellSharedWorkerToShutdown()) { return }
    // NOTE: 1000 is arbitrary
    await wait(500)
    if (!await initSharedWorker('relaunch-after-close-sharedworker.js')) { return }
  }

  log('PASS: Successfully shutdown and reconstructed the SharedWorker many times')
  testComplete("PASS: Successfully shutdown and reconstructed the SharedWorker many times")
}

// UTILITY FUNCTIONS
async function initSharedWorker(workerURL) {
  count += 1
  log('    Attempting to boot the shared worker')

  const def = deferredWithTimeout(3000)

  sharedWorker = new SharedWorker(workerURL)

  const connectedListener = e => {
    if (e.data === 'connected') {
      def.resolve()
    }
  }

  sharedWorker.port.addEventListener('message', messageLogger)
  sharedWorker.port.addEventListener('message', connectedListener)
  sharedWorker.port.start()

  try {
    await def.promise
    log('    SharedWorker connected')
    return true
  } catch {
    log('FAIL: SharedWorker failed to connect')
    if (window.opener)
        testComplete("FAIL: SharedWorker failed to connect")
    return false
  } finally {
    sharedWorker.port.removeEventListener('message', connectedListener)
  }
}

function disonnectFromSharedWorker() {
  sharedWorker.port.removeEventListener('message', messageLogger)
  sharedWorker.port.close()
  sharedWorker = null
}

async function tellSharedWorkerToShutdown() {
  if (!sharedWorker) { return }

  log('    Telling the SharedWorker to shutdown')

  const def = deferredWithTimeout(3000)

  const shutdownListener = e => {
    if (e.data === 'shutdown') {
      def.resolve()
    }
  }

  sharedWorker.port.addEventListener('message', shutdownListener)
  sharedWorker.port.postMessage('shutdown')

  try {
    await def.promise
    log('    SharedWorker successfully shutdown')
    return true
  } catch {
    log('FAIL: SharedWorker timed out during shutdown')
    if (window.opener)
      testComplete("FAIL: SharedWorker timed out during shutdown")
    return false
  } finally {
    sharedWorker.port.removeEventListener('message', shutdownListener)
    disonnectFromSharedWorker()
  }
}

function log(msg) {
  if (opener)
    opener.console.log("PAGE 2: " + msg);
}

function deferredWithTimeout(amount) {
  let resolve, reject

  const promise = new Promise((re, rej) => {
    resolve = re
    reject = rej
    setTimeout(() => rej(new Error('timeout')), amount)
  })

  return {
    resolve,
    reject,
    promise
  }
}

async function wait(amount) {
  return await deferredWithTimeout(amount).promise.catch(() => {})
}

