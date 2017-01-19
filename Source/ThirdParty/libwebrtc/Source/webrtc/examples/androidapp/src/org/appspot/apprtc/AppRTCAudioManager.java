/*
 *  Copyright 2014 The WebRTC Project Authors. All rights reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.appspot.apprtc;

import org.appspot.apprtc.util.AppRTCUtils;

import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.content.SharedPreferences;
import android.content.pm.PackageManager;
import android.media.AudioManager;
import android.preference.PreferenceManager;
import android.util.Log;

import java.util.Collections;
import java.util.HashSet;
import java.util.Set;

/**
 * AppRTCAudioManager manages all audio related parts of the AppRTC demo.
 */
public class AppRTCAudioManager {
  private static final String TAG = "AppRTCAudioManager";
  private static final String SPEAKERPHONE_AUTO = "auto";
  private static final String SPEAKERPHONE_TRUE = "true";
  private static final String SPEAKERPHONE_FALSE = "false";

  /**
   * AudioDevice is the names of possible audio devices that we currently
   * support.
   */
  // TODO(henrika): add support for BLUETOOTH as well.
  public enum AudioDevice {
    SPEAKER_PHONE,
    WIRED_HEADSET,
    EARPIECE,
  }

  private final Context apprtcContext;
  private final Runnable onStateChangeListener;
  private boolean initialized = false;
  private AudioManager audioManager;
  private int savedAudioMode = AudioManager.MODE_INVALID;
  private boolean savedIsSpeakerPhoneOn = false;
  private boolean savedIsMicrophoneMute = false;

  private final AudioDevice defaultAudioDevice;

  // Contains speakerphone setting: auto, true or false
  private final String useSpeakerphone;

  // Proximity sensor object. It measures the proximity of an object in cm
  // relative to the view screen of a device and can therefore be used to
  // assist device switching (close to ear <=> use headset earpiece if
  // available, far from ear <=> use speaker phone).
  private AppRTCProximitySensor proximitySensor = null;

  // Contains the currently selected audio device.
  private AudioDevice selectedAudioDevice;

  // Contains a list of available audio devices. A Set collection is used to
  // avoid duplicate elements.
  private final Set<AudioDevice> audioDevices = new HashSet<AudioDevice>();

  // Broadcast receiver for wired headset intent broadcasts.
  private BroadcastReceiver wiredHeadsetReceiver;

  // Callback method for changes in audio focus.
  private AudioManager.OnAudioFocusChangeListener audioFocusChangeListener;

  // This method is called when the proximity sensor reports a state change,
  // e.g. from "NEAR to FAR" or from "FAR to NEAR".
  private void onProximitySensorChangedState() {
    if (!useSpeakerphone.equals(SPEAKERPHONE_AUTO)) {
      return;
    }

    // The proximity sensor should only be activated when there are exactly two
    // available audio devices.
    if (audioDevices.size() == 2 && audioDevices.contains(AppRTCAudioManager.AudioDevice.EARPIECE)
        && audioDevices.contains(AppRTCAudioManager.AudioDevice.SPEAKER_PHONE)) {
      if (proximitySensor.sensorReportsNearState()) {
        // Sensor reports that a "handset is being held up to a person's ear",
        // or "something is covering the light sensor".
        setAudioDevice(AppRTCAudioManager.AudioDevice.EARPIECE);
      } else {
        // Sensor reports that a "handset is removed from a person's ear", or
        // "the light sensor is no longer covered".
        setAudioDevice(AppRTCAudioManager.AudioDevice.SPEAKER_PHONE);
      }
    }
  }

  /** Construction */
  static AppRTCAudioManager create(Context context, Runnable deviceStateChangeListener) {
    return new AppRTCAudioManager(context, deviceStateChangeListener);
  }

  private AppRTCAudioManager(Context context, Runnable deviceStateChangeListener) {
    apprtcContext = context;
    onStateChangeListener = deviceStateChangeListener;
    audioManager = ((AudioManager) context.getSystemService(Context.AUDIO_SERVICE));

    SharedPreferences sharedPreferences = PreferenceManager.getDefaultSharedPreferences(context);
    useSpeakerphone = sharedPreferences.getString(context.getString(R.string.pref_speakerphone_key),
        context.getString(R.string.pref_speakerphone_default));

    if (useSpeakerphone.equals(SPEAKERPHONE_FALSE)) {
      defaultAudioDevice = AudioDevice.EARPIECE;
    } else {
      defaultAudioDevice = AudioDevice.SPEAKER_PHONE;
    }

    // Create and initialize the proximity sensor.
    // Tablet devices (e.g. Nexus 7) does not support proximity sensors.
    // Note that, the sensor will not be active until start() has been called.
    proximitySensor = AppRTCProximitySensor.create(context, new Runnable() {
      // This method will be called each time a state change is detected.
      // Example: user holds his hand over the device (closer than ~5 cm),
      // or removes his hand from the device.
      public void run() {
        onProximitySensorChangedState();
      }
    });
    AppRTCUtils.logDeviceInfo(TAG);
  }

  public void init() {
    Log.d(TAG, "init");
    if (initialized) {
      return;
    }

    // Store current audio state so we can restore it when close() is called.
    savedAudioMode = audioManager.getMode();
    savedIsSpeakerPhoneOn = audioManager.isSpeakerphoneOn();
    savedIsMicrophoneMute = audioManager.isMicrophoneMute();

    // Create an AudioManager.OnAudioFocusChangeListener instance.
    audioFocusChangeListener = new AudioManager.OnAudioFocusChangeListener() {
      // Called on the listener to notify if the audio focus for this listener has been changed.
      // The |focusChange| value indicates whether the focus was gained, whether the focus was lost,
      // and whether that loss is transient, or whether the new focus holder will hold it for an
      // unknown amount of time.
      // TODO(henrika): possibly extend support of handling audio-focus changes. Only contains
      // logging for now.
      @Override
      public void onAudioFocusChange(int focusChange) {
        String typeOfChange = "AUDIOFOCUS_NOT_DEFINED";
        switch (focusChange) {
          case AudioManager.AUDIOFOCUS_GAIN:
            typeOfChange = "AUDIOFOCUS_GAIN";
            break;
          case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT:
            typeOfChange = "AUDIOFOCUS_GAIN_TRANSIENT";
            break;
          case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE:
            typeOfChange = "AUDIOFOCUS_GAIN_TRANSIENT_EXCLUSIVE";
            break;
          case AudioManager.AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK:
            typeOfChange = "AUDIOFOCUS_GAIN_TRANSIENT_MAY_DUCK";
            break;
          case AudioManager.AUDIOFOCUS_LOSS:
            typeOfChange = "AUDIOFOCUS_LOSS";
            break;
          case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT:
            typeOfChange = "AUDIOFOCUS_LOSS_TRANSIENT";
            break;
          case AudioManager.AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK:
            typeOfChange = "AUDIOFOCUS_LOSS_TRANSIENT_CAN_DUCK";
            break;
          default:
            typeOfChange = "AUDIOFOCUS_INVALID";
            break;
        }
        Log.d(TAG, "onAudioFocusChange: " + typeOfChange);
      }
    };

    // Request audio playout focus (without ducking) and install listener for changes in focus.
    int result = audioManager.requestAudioFocus(audioFocusChangeListener,
        AudioManager.STREAM_VOICE_CALL, AudioManager.AUDIOFOCUS_GAIN_TRANSIENT);
    if (result == AudioManager.AUDIOFOCUS_REQUEST_GRANTED) {
      Log.d(TAG, "Audio focus request granted for VOICE_CALL streams");
    } else {
      Log.e(TAG, "Audio focus request failed");
    }

    // Start by setting MODE_IN_COMMUNICATION as default audio mode. It is
    // required to be in this mode when playout and/or recording starts for
    // best possible VoIP performance.
    // TODO(henrika): we migh want to start with RINGTONE mode here instead.
    audioManager.setMode(AudioManager.MODE_IN_COMMUNICATION);

    // Always disable microphone mute during a WebRTC call.
    setMicrophoneMute(false);

    // Do initial selection of audio device. This setting can later be changed
    // either by adding/removing a wired headset or by covering/uncovering the
    // proximity sensor.
    updateAudioDeviceState(hasWiredHeadset());

    // Register receiver for broadcast intents related to adding/removing a
    // wired headset (Intent.ACTION_HEADSET_PLUG).
    registerForWiredHeadsetIntentBroadcast();

    initialized = true;
  }

  public void close() {
    Log.d(TAG, "close");
    if (!initialized) {
      return;
    }

    unregisterForWiredHeadsetIntentBroadcast();

    // Restore previously stored audio states.
    setSpeakerphoneOn(savedIsSpeakerPhoneOn);
    setMicrophoneMute(savedIsMicrophoneMute);
    audioManager.setMode(savedAudioMode);

    // Abandon audio focus. Gives the previous focus owner, if any, focus.
    audioManager.abandonAudioFocus(audioFocusChangeListener);
    audioFocusChangeListener = null;
    Log.d(TAG, "Abandoned audio focus for VOICE_CALL streams");

    if (proximitySensor != null) {
      proximitySensor.stop();
      proximitySensor = null;
    }

    initialized = false;
  }

  /** Changes selection of the currently active audio device. */
  public void setAudioDevice(AudioDevice device) {
    Log.d(TAG, "setAudioDevice(device=" + device + ")");
    AppRTCUtils.assertIsTrue(audioDevices.contains(device));

    switch (device) {
      case SPEAKER_PHONE:
        setSpeakerphoneOn(true);
        selectedAudioDevice = AudioDevice.SPEAKER_PHONE;
        break;
      case EARPIECE:
        setSpeakerphoneOn(false);
        selectedAudioDevice = AudioDevice.EARPIECE;
        break;
      case WIRED_HEADSET:
        setSpeakerphoneOn(false);
        selectedAudioDevice = AudioDevice.WIRED_HEADSET;
        break;
      default:
        Log.e(TAG, "Invalid audio device selection");
        break;
    }
    onAudioManagerChangedState();
  }

  /** Returns current set of available/selectable audio devices. */
  public Set<AudioDevice> getAudioDevices() {
    return Collections.unmodifiableSet(new HashSet<AudioDevice>(audioDevices));
  }

  /** Returns the currently selected audio device. */
  public AudioDevice getSelectedAudioDevice() {
    return selectedAudioDevice;
  }

  /**
   * Registers receiver for the broadcasted intent when a wired headset is
   * plugged in or unplugged. The received intent will have an extra
   * 'state' value where 0 means unplugged, and 1 means plugged.
   */
  private void registerForWiredHeadsetIntentBroadcast() {
    IntentFilter filter = new IntentFilter(Intent.ACTION_HEADSET_PLUG);

    /** Receiver which handles changes in wired headset availability. */
    wiredHeadsetReceiver = new BroadcastReceiver() {
      private static final int STATE_UNPLUGGED = 0;
      private static final int STATE_PLUGGED = 1;
      private static final int HAS_NO_MIC = 0;
      private static final int HAS_MIC = 1;

      @Override
      public void onReceive(Context context, Intent intent) {
        int state = intent.getIntExtra("state", STATE_UNPLUGGED);
        int microphone = intent.getIntExtra("microphone", HAS_NO_MIC);
        String name = intent.getStringExtra("name");
        Log.d(TAG, "BroadcastReceiver.onReceive" + AppRTCUtils.getThreadInfo() + ": "
                + "a=" + intent.getAction() + ", s="
                + (state == STATE_UNPLUGGED ? "unplugged" : "plugged") + ", m="
                + (microphone == HAS_MIC ? "mic" : "no mic") + ", n=" + name + ", sb="
                + isInitialStickyBroadcast());

        boolean hasWiredHeadset = (state == STATE_PLUGGED);
        switch (state) {
          case STATE_UNPLUGGED:
            updateAudioDeviceState(hasWiredHeadset);
            break;
          case STATE_PLUGGED:
            if (selectedAudioDevice != AudioDevice.WIRED_HEADSET) {
              updateAudioDeviceState(hasWiredHeadset);
            }
            break;
          default:
            Log.e(TAG, "Invalid state");
            break;
        }
      }
    };

    apprtcContext.registerReceiver(wiredHeadsetReceiver, filter);
  }

  /** Unregister receiver for broadcasted ACTION_HEADSET_PLUG intent. */
  private void unregisterForWiredHeadsetIntentBroadcast() {
    apprtcContext.unregisterReceiver(wiredHeadsetReceiver);
    wiredHeadsetReceiver = null;
  }

  /** Sets the speaker phone mode. */
  private void setSpeakerphoneOn(boolean on) {
    boolean wasOn = audioManager.isSpeakerphoneOn();
    if (wasOn == on) {
      return;
    }
    audioManager.setSpeakerphoneOn(on);
  }

  /** Sets the microphone mute state. */
  private void setMicrophoneMute(boolean on) {
    boolean wasMuted = audioManager.isMicrophoneMute();
    if (wasMuted == on) {
      return;
    }
    audioManager.setMicrophoneMute(on);
  }

  /** Gets the current earpiece state. */
  private boolean hasEarpiece() {
    return apprtcContext.getPackageManager().hasSystemFeature(PackageManager.FEATURE_TELEPHONY);
  }

  /**
   * Checks whether a wired headset is connected or not.
   * This is not a valid indication that audio playback is actually over
   * the wired headset as audio routing depends on other conditions. We
   * only use it as an early indicator (during initialization) of an attached
   * wired headset.
   */
  @Deprecated
  private boolean hasWiredHeadset() {
    return audioManager.isWiredHeadsetOn();
  }

  /** Update list of possible audio devices and make new device selection. */
  private void updateAudioDeviceState(boolean hasWiredHeadset) {
    // Update the list of available audio devices.
    audioDevices.clear();
    if (hasWiredHeadset) {
      // If a wired headset is connected, then it is the only possible option.
      audioDevices.add(AudioDevice.WIRED_HEADSET);
    } else {
      // No wired headset, hence the audio-device list can contain speaker
      // phone (on a tablet), or speaker phone and earpiece (on mobile phone).
      audioDevices.add(AudioDevice.SPEAKER_PHONE);
      if (hasEarpiece()) {
        audioDevices.add(AudioDevice.EARPIECE);
      }
    }
    Log.d(TAG, "audioDevices: " + audioDevices);

    // Switch to correct audio device given the list of available audio devices.
    if (hasWiredHeadset) {
      setAudioDevice(AudioDevice.WIRED_HEADSET);
    } else {
      setAudioDevice(defaultAudioDevice);
    }
  }

  /** Called each time a new audio device has been added or removed. */
  private void onAudioManagerChangedState() {
    Log.d(TAG, "onAudioManagerChangedState: devices=" + audioDevices + ", selected="
            + selectedAudioDevice);

    // Enable the proximity sensor if there are two available audio devices
    // in the list. Given the current implementation, we know that the choice
    // will then be between EARPIECE and SPEAKER_PHONE.
    if (audioDevices.size() == 2) {
      AppRTCUtils.assertIsTrue(audioDevices.contains(AudioDevice.EARPIECE)
          && audioDevices.contains(AudioDevice.SPEAKER_PHONE));
      // Start the proximity sensor.
      proximitySensor.start();
    } else if (audioDevices.size() == 1) {
      // Stop the proximity sensor since it is no longer needed.
      proximitySensor.stop();
    } else {
      Log.e(TAG, "Invalid device list");
    }

    if (onStateChangeListener != null) {
      // Run callback to notify a listening client. The client can then
      // use public getters to query the new state.
      onStateChangeListener.run();
    }
  }
}
