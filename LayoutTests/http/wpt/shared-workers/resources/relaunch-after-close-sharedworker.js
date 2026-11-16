const connections = new Set()

self.onconnect = e => {
  const port = e.ports[0]
  const ref = new WeakRef(port)

  port.onmessage = e => {
    if (e.data === 'shutdown') {
      shutdown()
    }
  }

  connections.add(ref)
  port.postMessage('connected')
}

function shutdown() {
  broadcast('shutdown')

  try {
    for (const ref of connections) {
      const port = ref.deref()

      if (port) {
        try {
          port.onmessage = null
          port.close()
        } catch { }
      }
    }
  } finally {
    self.close()
  }
}

function broadcast(msg) {
  for (const ref of connections) {
    sendMessageToPortRef(ref, msg)
  }
}

function sendMessageToPortRef(ref, msg) {
  const port = ref.deref()
  let failed = false

  try {
    port?.postMessage(msg)
  } catch {
    failed = true
  }

  if (!port || failed) {
    connections.delete(ref)
  }
}

