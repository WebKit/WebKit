/*
 *  Copyright 2016 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import android.support.annotation.Nullable;
import android.util.Log;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.io.OutputStreamWriter;
import java.io.PrintWriter;
import java.net.InetAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.UnknownHostException;
import java.nio.charset.Charset;
import java.util.concurrent.ExecutorService;
import org.webrtc.ThreadUtils;

/**
 * Replacement for WebSocketChannelClient for direct communication between two IP addresses. Handles
 * the signaling between the two clients using a TCP connection.
 * <p>
 * All public methods should be called from a looper executor thread
 * passed in a constructor, otherwise exception will be thrown.
 * All events are dispatched on the same thread.
 */
public class TCPChannelClient {
  private static final String TAG = "TCPChannelClient";

  private final ExecutorService executor;
  private final ThreadUtils.ThreadChecker executorThreadCheck;
  private final TCPChannelEvents eventListener;
  private TCPSocket socket;

  /**
   * Callback interface for messages delivered on TCP Connection. All callbacks are invoked from the
   * looper executor thread.
   */
  public interface TCPChannelEvents {
    void onTCPConnected(boolean server);
    void onTCPMessage(String message);
    void onTCPError(String description);
    void onTCPClose();
  }

  /**
   * Initializes the TCPChannelClient. If IP is a local IP address, starts a listening server on
   * that IP. If not, instead connects to the IP.
   *
   * @param eventListener Listener that will receive events from the client.
   * @param ip            IP address to listen on or connect to.
   * @param port          Port to listen on or connect to.
   */
  public TCPChannelClient(
      ExecutorService executor, TCPChannelEvents eventListener, String ip, int port) {
    this.executor = executor;
    executorThreadCheck = new ThreadUtils.ThreadChecker();
    executorThreadCheck.detachThread();
    this.eventListener = eventListener;

    InetAddress address;
    try {
      address = InetAddress.getByName(ip);
    } catch (UnknownHostException e) {
      reportError("Invalid IP address.");
      return;
    }

    if (address.isAnyLocalAddress()) {
      socket = new TCPSocketServer(address, port);
    } else {
      socket = new TCPSocketClient(address, port);
    }

    socket.start();
  }

  /**
   * Disconnects the client if not already disconnected. This will fire the onTCPClose event.
   */
  public void disconnect() {
    executorThreadCheck.checkIsOnValidThread();

    socket.disconnect();
  }

  /**
   * Sends a message on the socket.
   *
   * @param message Message to be sent.
   */
  public void send(String message) {
    executorThreadCheck.checkIsOnValidThread();

    socket.send(message);
  }

  /**
   * Helper method for firing onTCPError events. Calls onTCPError on the executor thread.
   */
  private void reportError(final String message) {
    Log.e(TAG, "TCP Error: " + message);
    executor.execute(new Runnable() {
      @Override
      public void run() {
        eventListener.onTCPError(message);
      }
    });
  }

  /**
   * Base class for server and client sockets. Contains a listening thread that will call
   * eventListener.onTCPMessage on new messages.
   */
  private abstract class TCPSocket extends Thread {
    // Lock for editing out and rawSocket
    protected final Object rawSocketLock;
    @Nullable
    private PrintWriter out;
    @Nullable
    private Socket rawSocket;

    /**
     * Connect to the peer, potentially a slow operation.
     *
     * @return Socket connection, null if connection failed.
     */
    @Nullable
    public abstract Socket connect();

    /** Returns true if sockets is a server rawSocket. */
    public abstract boolean isServer();

    TCPSocket() {
      rawSocketLock = new Object();
    }

    /**
     * The listening thread.
     */
    @Override
    public void run() {
      Log.d(TAG, "Listening thread started...");

      // Receive connection to temporary variable first, so we don't block.
      Socket tempSocket = connect();
      BufferedReader in;

      Log.d(TAG, "TCP connection established.");

      synchronized (rawSocketLock) {
        if (rawSocket != null) {
          Log.e(TAG, "Socket already existed and will be replaced.");
        }

        rawSocket = tempSocket;

        // Connecting failed, error has already been reported, just exit.
        if (rawSocket == null) {
          return;
        }

        try {
          out = new PrintWriter(
              new OutputStreamWriter(rawSocket.getOutputStream(), Charset.forName("UTF-8")), true);
          in = new BufferedReader(
              new InputStreamReader(rawSocket.getInputStream(), Charset.forName("UTF-8")));
        } catch (IOException e) {
          reportError("Failed to open IO on rawSocket: " + e.getMessage());
          return;
        }
      }

      Log.v(TAG, "Execute onTCPConnected");
      executor.execute(new Runnable() {
        @Override
        public void run() {
          Log.v(TAG, "Run onTCPConnected");
          eventListener.onTCPConnected(isServer());
        }
      });

      while (true) {
        final String message;
        try {
          message = in.readLine();
        } catch (IOException e) {
          synchronized (rawSocketLock) {
            // If socket was closed, this is expected.
            if (rawSocket == null) {
              break;
            }
          }

          reportError("Failed to read from rawSocket: " + e.getMessage());
          break;
        }

        // No data received, rawSocket probably closed.
        if (message == null) {
          break;
        }

        executor.execute(new Runnable() {
          @Override
          public void run() {
            Log.v(TAG, "Receive: " + message);
            eventListener.onTCPMessage(message);
          }
        });
      }

      Log.d(TAG, "Receiving thread exiting...");

      // Close the rawSocket if it is still open.
      disconnect();
    }

    /** Closes the rawSocket if it is still open. Also fires the onTCPClose event. */
    public void disconnect() {
      try {
        synchronized (rawSocketLock) {
          if (rawSocket != null) {
            rawSocket.close();
            rawSocket = null;
            out = null;

            executor.execute(new Runnable() {
              @Override
              public void run() {
                eventListener.onTCPClose();
              }
            });
          }
        }
      } catch (IOException e) {
        reportError("Failed to close rawSocket: " + e.getMessage());
      }
    }

    /**
     * Sends a message on the socket. Should only be called on the executor thread.
     */
    public void send(String message) {
      Log.v(TAG, "Send: " + message);

      synchronized (rawSocketLock) {
        if (out == null) {
          reportError("Sending data on closed socket.");
          return;
        }

        out.write(message + "\n");
        out.flush();
      }
    }
  }

  private class TCPSocketServer extends TCPSocket {
    // Server socket is also guarded by rawSocketLock.
    @Nullable
    private ServerSocket serverSocket;

    final private InetAddress address;
    final private int port;

    public TCPSocketServer(InetAddress address, int port) {
      this.address = address;
      this.port = port;
    }

    /** Opens a listening socket and waits for a connection. */
    @Nullable
    @Override
    public Socket connect() {
      Log.d(TAG, "Listening on [" + address.getHostAddress() + "]:" + Integer.toString(port));

      final ServerSocket tempSocket;
      try {
        tempSocket = new ServerSocket(port, 0, address);
      } catch (IOException e) {
        reportError("Failed to create server socket: " + e.getMessage());
        return null;
      }

      synchronized (rawSocketLock) {
        if (serverSocket != null) {
          Log.e(TAG, "Server rawSocket was already listening and new will be opened.");
        }

        serverSocket = tempSocket;
      }

      try {
        return tempSocket.accept();
      } catch (IOException e) {
        reportError("Failed to receive connection: " + e.getMessage());
        return null;
      }
    }

    /** Closes the listening socket and calls super. */
    @Override
    public void disconnect() {
      try {
        synchronized (rawSocketLock) {
          if (serverSocket != null) {
            serverSocket.close();
            serverSocket = null;
          }
        }
      } catch (IOException e) {
        reportError("Failed to close server socket: " + e.getMessage());
      }

      super.disconnect();
    }

    @Override
    public boolean isServer() {
      return true;
    }
  }

  private class TCPSocketClient extends TCPSocket {
    final private InetAddress address;
    final private int port;

    public TCPSocketClient(InetAddress address, int port) {
      this.address = address;
      this.port = port;
    }

    /** Connects to the peer. */
    @Nullable
    @Override
    public Socket connect() {
      Log.d(TAG, "Connecting to [" + address.getHostAddress() + "]:" + Integer.toString(port));

      try {
        return new Socket(address, port);
      } catch (IOException e) {
        reportError("Failed to connect: " + e.getMessage());
        return null;
      }
    }

    @Override
    public boolean isServer() {
      return false;
    }
  }
}
