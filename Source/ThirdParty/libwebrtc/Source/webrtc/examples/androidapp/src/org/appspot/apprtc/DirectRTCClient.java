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

import javax.annotation.Nullable;
import android.util.Log;

import org.json.JSONArray;
import org.json.JSONException;
import org.json.JSONObject;
import org.webrtc.IceCandidate;
import org.webrtc.PeerConnection;
import org.webrtc.SessionDescription;

import java.util.ArrayList;
import java.util.concurrent.ExecutorService;
import java.util.concurrent.Executors;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

/**
 * Implementation of AppRTCClient that uses direct TCP connection as the signaling channel.
 * This eliminates the need for an external server. This class does not support loopback
 * connections.
 */
public class DirectRTCClient implements AppRTCClient, TCPChannelClient.TCPChannelEvents {
  private static final String TAG = "DirectRTCClient";
  private static final int DEFAULT_PORT = 8888;

  // Regex pattern used for checking if room id looks like an IP.
  static final Pattern IP_PATTERN = Pattern.compile("("
      // IPv4
      + "((\\d+\\.){3}\\d+)|"
      // IPv6
      + "\\[((([0-9a-fA-F]{1,4}:)*[0-9a-fA-F]{1,4})?::"
      + "(([0-9a-fA-F]{1,4}:)*[0-9a-fA-F]{1,4})?)\\]|"
      + "\\[(([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4})\\]|"
      // IPv6 without []
      + "((([0-9a-fA-F]{1,4}:)*[0-9a-fA-F]{1,4})?::(([0-9a-fA-F]{1,4}:)*[0-9a-fA-F]{1,4})?)|"
      + "(([0-9a-fA-F]{1,4}:){7}[0-9a-fA-F]{1,4})|"
      // Literals
      + "localhost"
      + ")"
      // Optional port number
      + "(:(\\d+))?");

  private final ExecutorService executor;
  private final SignalingEvents events;
  @Nullable
  private TCPChannelClient tcpClient;
  private RoomConnectionParameters connectionParameters;

  private enum ConnectionState { NEW, CONNECTED, CLOSED, ERROR }

  // All alterations of the room state should be done from inside the looper thread.
  private ConnectionState roomState;

  public DirectRTCClient(SignalingEvents events) {
    this.events = events;

    executor = Executors.newSingleThreadExecutor();
    roomState = ConnectionState.NEW;
  }

  /**
   * Connects to the room, roomId in connectionsParameters is required. roomId must be a valid
   * IP address matching IP_PATTERN.
   */
  @Override
  public void connectToRoom(RoomConnectionParameters connectionParameters) {
    this.connectionParameters = connectionParameters;

    if (connectionParameters.loopback) {
      reportError("Loopback connections aren't supported by DirectRTCClient.");
    }

    executor.execute(new Runnable() {
      @Override
      public void run() {
        connectToRoomInternal();
      }
    });
  }

  @Override
  public void disconnectFromRoom() {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        disconnectFromRoomInternal();
      }
    });
  }

  /**
   * Connects to the room.
   *
   * Runs on the looper thread.
   */
  private void connectToRoomInternal() {
    this.roomState = ConnectionState.NEW;

    String endpoint = connectionParameters.roomId;

    Matcher matcher = IP_PATTERN.matcher(endpoint);
    if (!matcher.matches()) {
      reportError("roomId must match IP_PATTERN for DirectRTCClient.");
      return;
    }

    String ip = matcher.group(1);
    String portStr = matcher.group(matcher.groupCount());
    int port;

    if (portStr != null) {
      try {
        port = Integer.parseInt(portStr);
      } catch (NumberFormatException e) {
        reportError("Invalid port number: " + portStr);
        return;
      }
    } else {
      port = DEFAULT_PORT;
    }

    tcpClient = new TCPChannelClient(executor, this, ip, port);
  }

  /**
   * Disconnects from the room.
   *
   * Runs on the looper thread.
   */
  private void disconnectFromRoomInternal() {
    roomState = ConnectionState.CLOSED;

    if (tcpClient != null) {
      tcpClient.disconnect();
      tcpClient = null;
    }
    executor.shutdown();
  }

  @Override
  public void sendOfferSdp(final SessionDescription sdp) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (roomState != ConnectionState.CONNECTED) {
          reportError("Sending offer SDP in non connected state.");
          return;
        }
        JSONObject json = new JSONObject();
        jsonPut(json, "sdp", sdp.description);
        jsonPut(json, "type", "offer");
        sendMessage(json.toString());
      }
    });
  }

  @Override
  public void sendAnswerSdp(final SessionDescription sdp) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        JSONObject json = new JSONObject();
        jsonPut(json, "sdp", sdp.description);
        jsonPut(json, "type", "answer");
        sendMessage(json.toString());
      }
    });
  }

  @Override
  public void sendLocalIceCandidate(final IceCandidate candidate) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        JSONObject json = new JSONObject();
        jsonPut(json, "type", "candidate");
        jsonPut(json, "label", candidate.sdpMLineIndex);
        jsonPut(json, "id", candidate.sdpMid);
        jsonPut(json, "candidate", candidate.sdp);

        if (roomState != ConnectionState.CONNECTED) {
          reportError("Sending ICE candidate in non connected state.");
          return;
        }
        sendMessage(json.toString());
      }
    });
  }

  /** Send removed Ice candidates to the other participant. */
  @Override
  public void sendLocalIceCandidateRemovals(final IceCandidate[] candidates) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        JSONObject json = new JSONObject();
        jsonPut(json, "type", "remove-candidates");
        JSONArray jsonArray = new JSONArray();
        for (final IceCandidate candidate : candidates) {
          jsonArray.put(toJsonCandidate(candidate));
        }
        jsonPut(json, "candidates", jsonArray);

        if (roomState != ConnectionState.CONNECTED) {
          reportError("Sending ICE candidate removals in non connected state.");
          return;
        }
        sendMessage(json.toString());
      }
    });
  }

  // -------------------------------------------------------------------
  // TCPChannelClient event handlers

  /**
   * If the client is the server side, this will trigger onConnectedToRoom.
   */
  @Override
  public void onTCPConnected(boolean isServer) {
    if (isServer) {
      roomState = ConnectionState.CONNECTED;

      SignalingParameters parameters = new SignalingParameters(
          // Ice servers are not needed for direct connections.
          new ArrayList<>(),
          isServer, // Server side acts as the initiator on direct connections.
          null, // clientId
          null, // wssUrl
          null, // wwsPostUrl
          null, // offerSdp
          null // iceCandidates
          );
      events.onConnectedToRoom(parameters);
    }
  }

  @Override
  public void onTCPMessage(String msg) {
    try {
      JSONObject json = new JSONObject(msg);
      String type = json.optString("type");
      if (type.equals("candidate")) {
        events.onRemoteIceCandidate(toJavaCandidate(json));
      } else if (type.equals("remove-candidates")) {
        JSONArray candidateArray = json.getJSONArray("candidates");
        IceCandidate[] candidates = new IceCandidate[candidateArray.length()];
        for (int i = 0; i < candidateArray.length(); ++i) {
          candidates[i] = toJavaCandidate(candidateArray.getJSONObject(i));
        }
        events.onRemoteIceCandidatesRemoved(candidates);
      } else if (type.equals("answer")) {
        SessionDescription sdp = new SessionDescription(
            SessionDescription.Type.fromCanonicalForm(type), json.getString("sdp"));
        events.onRemoteDescription(sdp);
      } else if (type.equals("offer")) {
        SessionDescription sdp = new SessionDescription(
            SessionDescription.Type.fromCanonicalForm(type), json.getString("sdp"));

        SignalingParameters parameters = new SignalingParameters(
            // Ice servers are not needed for direct connections.
            new ArrayList<>(),
            false, // This code will only be run on the client side. So, we are not the initiator.
            null, // clientId
            null, // wssUrl
            null, // wssPostUrl
            sdp, // offerSdp
            null // iceCandidates
            );
        roomState = ConnectionState.CONNECTED;
        events.onConnectedToRoom(parameters);
      } else {
        reportError("Unexpected TCP message: " + msg);
      }
    } catch (JSONException e) {
      reportError("TCP message JSON parsing error: " + e.toString());
    }
  }

  @Override
  public void onTCPError(String description) {
    reportError("TCP connection error: " + description);
  }

  @Override
  public void onTCPClose() {
    events.onChannelClose();
  }

  // --------------------------------------------------------------------
  // Helper functions.
  private void reportError(final String errorMessage) {
    Log.e(TAG, errorMessage);
    executor.execute(new Runnable() {
      @Override
      public void run() {
        if (roomState != ConnectionState.ERROR) {
          roomState = ConnectionState.ERROR;
          events.onChannelError(errorMessage);
        }
      }
    });
  }

  private void sendMessage(final String message) {
    executor.execute(new Runnable() {
      @Override
      public void run() {
        tcpClient.send(message);
      }
    });
  }

  // Put a |key|->|value| mapping in |json|.
  private static void jsonPut(JSONObject json, String key, Object value) {
    try {
      json.put(key, value);
    } catch (JSONException e) {
      throw new RuntimeException(e);
    }
  }

  // Converts a Java candidate to a JSONObject.
  private static JSONObject toJsonCandidate(final IceCandidate candidate) {
    JSONObject json = new JSONObject();
    jsonPut(json, "label", candidate.sdpMLineIndex);
    jsonPut(json, "id", candidate.sdpMid);
    jsonPut(json, "candidate", candidate.sdp);
    return json;
  }

  // Converts a JSON candidate to a Java object.
  private static IceCandidate toJavaCandidate(JSONObject json) throws JSONException {
    return new IceCandidate(
        json.getString("id"), json.getInt("label"), json.getString("candidate"));
  }
}
