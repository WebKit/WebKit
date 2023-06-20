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

import static org.junit.Assert.assertEquals;
import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.never;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import android.bluetooth.BluetoothAdapter;
import android.bluetooth.BluetoothDevice;
import android.bluetooth.BluetoothHeadset;
import android.bluetooth.BluetoothProfile;
import android.content.BroadcastReceiver;
import android.content.Context;
import android.content.Intent;
import android.content.IntentFilter;
import android.media.AudioManager;
import android.util.Log;
import androidx.test.core.app.ApplicationProvider;
import java.util.ArrayList;
import java.util.List;
import org.appspot.apprtc.AppRTCBluetoothManager.State;
import org.junit.Before;
import org.junit.Test;
import org.junit.runner.RunWith;
import org.robolectric.annotation.Config;
import org.robolectric.shadows.ShadowLog;
import org.robolectric.RobolectricTestRunner;

/**
 * Verifies basic behavior of the AppRTCBluetoothManager class.
 * Note that the test object uses an AppRTCAudioManager (injected in ctor),
 * but a mocked version is used instead. Hence, the parts "driven" by the AppRTC
 * audio manager are not included in this test.
 */
@RunWith(RobolectricTestRunner.class)
@Config(manifest = Config.NONE)
public class BluetoothManagerTest {
  private static final String TAG = "BluetoothManagerTest";
  private static final String BLUETOOTH_TEST_DEVICE_NAME = "BluetoothTestDevice";

  private BroadcastReceiver bluetoothHeadsetStateReceiver;
  private BluetoothProfile.ServiceListener bluetoothServiceListener;
  private BluetoothHeadset mockedBluetoothHeadset;
  private BluetoothDevice mockedBluetoothDevice;
  private List<BluetoothDevice> mockedBluetoothDeviceList;
  private AppRTCBluetoothManager bluetoothManager;
  private AppRTCAudioManager mockedAppRtcAudioManager;
  private AudioManager mockedAudioManager;
  private Context context;

  @Before
  public void setUp() {
    ShadowLog.stream = System.out;
    context = ApplicationProvider.getApplicationContext();
    mockedAppRtcAudioManager = mock(AppRTCAudioManager.class);
    mockedAudioManager = mock(AudioManager.class);
    mockedBluetoothHeadset = mock(BluetoothHeadset.class);
    mockedBluetoothDevice = mock(BluetoothDevice.class);
    mockedBluetoothDeviceList = new ArrayList<BluetoothDevice>();

    // Simulate that bluetooth SCO audio is available by default.
    when(mockedAudioManager.isBluetoothScoAvailableOffCall()).thenReturn(true);

    // Create the test object and override protected methods for this test.
    bluetoothManager = new AppRTCBluetoothManager(context, mockedAppRtcAudioManager) {
      @Override
      protected AudioManager getAudioManager(Context context) {
        Log.d(TAG, "getAudioManager");
        return mockedAudioManager;
      }

      @Override
      protected void registerReceiver(BroadcastReceiver receiver, IntentFilter filter) {
        Log.d(TAG, "registerReceiver");
        if (filter.hasAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED)
            && filter.hasAction(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED)) {
          // Gives access to the real broadcast receiver so the test can use it.
          bluetoothHeadsetStateReceiver = receiver;
        }
      }

      @Override
      protected void unregisterReceiver(BroadcastReceiver receiver) {
        Log.d(TAG, "unregisterReceiver");
        if (receiver == bluetoothHeadsetStateReceiver) {
          bluetoothHeadsetStateReceiver = null;
        }
      }

      @Override
      protected boolean getBluetoothProfileProxy(
          Context context, BluetoothProfile.ServiceListener listener, int profile) {
        Log.d(TAG, "getBluetoothProfileProxy");
        if (profile == BluetoothProfile.HEADSET) {
          // Allows the test to access the real Bluetooth service listener object.
          bluetoothServiceListener = listener;
        }
        return true;
      }

      @Override
      protected boolean hasPermission(Context context, String permission) {
        Log.d(TAG, "hasPermission(" + permission + ")");
        // Ensure that the client asks for Bluetooth permission.
        return android.Manifest.permission.BLUETOOTH.equals(permission);
      }

      @Override
      protected void logBluetoothAdapterInfo(BluetoothAdapter localAdapter) {
        // Do nothing in tests. No need to mock BluetoothAdapter.
      }
    };
  }

  // Verify that Bluetooth service listener for headset profile is properly initialized.
  @Test
  public void testBluetoothServiceListenerInitialized() {
    bluetoothManager.start();
    assertNotNull(bluetoothServiceListener);
    verify(mockedAppRtcAudioManager, never()).updateAudioDeviceState();
  }

  // Verify that broadcast receivers for Bluetooth SCO audio state and Bluetooth headset state
  // are properly registered and unregistered.
  @Test
  public void testBluetoothBroadcastReceiversAreRegistered() {
    bluetoothManager.start();
    assertNotNull(bluetoothHeadsetStateReceiver);
    bluetoothManager.stop();
    assertNull(bluetoothHeadsetStateReceiver);
  }

  // Verify that the Bluetooth manager starts and stops with correct states.
  @Test
  public void testBluetoothDefaultStartStopStates() {
    bluetoothManager.start();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
    bluetoothManager.stop();
    assertEquals(bluetoothManager.getState(), State.UNINITIALIZED);
  }

  // Verify correct state after receiving BluetoothServiceListener.onServiceConnected()
  // when no BT device is enabled.
  @Test
  public void testBluetoothServiceListenerConnectedWithNoHeadset() {
    bluetoothManager.start();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
    simulateBluetoothServiceConnectedWithNoConnectedHeadset();
    verify(mockedAppRtcAudioManager, times(1)).updateAudioDeviceState();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
  }

  // Verify correct state after receiving BluetoothServiceListener.onServiceConnected()
  // when one emulated (test) BT device is enabled. Android does not support more than
  // one connected BT headset.
  @Test
  public void testBluetoothServiceListenerConnectedWithHeadset() {
    bluetoothManager.start();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
    simulateBluetoothServiceConnectedWithConnectedHeadset();
    verify(mockedAppRtcAudioManager, times(1)).updateAudioDeviceState();
    assertEquals(bluetoothManager.getState(), State.HEADSET_AVAILABLE);
  }

  // Verify correct state after receiving BluetoothProfile.ServiceListener.onServiceDisconnected().
  @Test
  public void testBluetoothServiceListenerDisconnected() {
    bluetoothManager.start();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
    simulateBluetoothServiceDisconnected();
    verify(mockedAppRtcAudioManager, times(1)).updateAudioDeviceState();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
  }

  // Verify correct state after BluetoothServiceListener.onServiceConnected() and
  // the intent indicating that the headset is actually connected. Both these callbacks
  // results in calls to updateAudioDeviceState() on the AppRTC audio manager.
  // No BT SCO is enabled here to keep the test limited.
  @Test
  public void testBluetoothHeadsetConnected() {
    bluetoothManager.start();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
    simulateBluetoothServiceConnectedWithConnectedHeadset();
    simulateBluetoothHeadsetConnected();
    verify(mockedAppRtcAudioManager, times(2)).updateAudioDeviceState();
    assertEquals(bluetoothManager.getState(), State.HEADSET_AVAILABLE);
  }

  // Verify correct state sequence for a case when a BT headset is available,
  // followed by BT SCO audio being enabled and then stopped.
  @Test
  public void testBluetoothScoAudioStartAndStop() {
    bluetoothManager.start();
    assertEquals(bluetoothManager.getState(), State.HEADSET_UNAVAILABLE);
    simulateBluetoothServiceConnectedWithConnectedHeadset();
    assertEquals(bluetoothManager.getState(), State.HEADSET_AVAILABLE);
    bluetoothManager.startScoAudio();
    assertEquals(bluetoothManager.getState(), State.SCO_CONNECTING);
    simulateBluetoothScoConnectionConnected();
    assertEquals(bluetoothManager.getState(), State.SCO_CONNECTED);
    bluetoothManager.stopScoAudio();
    simulateBluetoothScoConnectionDisconnected();
    assertEquals(bluetoothManager.getState(), State.SCO_DISCONNECTING);
    bluetoothManager.stop();
    assertEquals(bluetoothManager.getState(), State.UNINITIALIZED);
    verify(mockedAppRtcAudioManager, times(3)).updateAudioDeviceState();
  }

  /**
   * Private helper methods.
   */
  private void simulateBluetoothServiceConnectedWithNoConnectedHeadset() {
    mockedBluetoothDeviceList.clear();
    when(mockedBluetoothHeadset.getConnectedDevices()).thenReturn(mockedBluetoothDeviceList);
    bluetoothServiceListener.onServiceConnected(BluetoothProfile.HEADSET, mockedBluetoothHeadset);
    // In real life, the AppRTC audio manager makes this call.
    bluetoothManager.updateDevice();
  }

  private void simulateBluetoothServiceConnectedWithConnectedHeadset() {
    mockedBluetoothDeviceList.clear();
    mockedBluetoothDeviceList.add(mockedBluetoothDevice);
    when(mockedBluetoothHeadset.getConnectedDevices()).thenReturn(mockedBluetoothDeviceList);
    when(mockedBluetoothDevice.getName()).thenReturn(BLUETOOTH_TEST_DEVICE_NAME);
    bluetoothServiceListener.onServiceConnected(BluetoothProfile.HEADSET, mockedBluetoothHeadset);
    // In real life, the AppRTC audio manager makes this call.
    bluetoothManager.updateDevice();
  }

  private void simulateBluetoothServiceDisconnected() {
    bluetoothServiceListener.onServiceDisconnected(BluetoothProfile.HEADSET);
  }

  private void simulateBluetoothHeadsetConnected() {
    Intent intent = new Intent();
    intent.setAction(BluetoothHeadset.ACTION_CONNECTION_STATE_CHANGED);
    intent.putExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_CONNECTED);
    bluetoothHeadsetStateReceiver.onReceive(context, intent);
  }

  private void simulateBluetoothScoConnectionConnected() {
    Intent intent = new Intent();
    intent.setAction(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED);
    intent.putExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_AUDIO_CONNECTED);
    bluetoothHeadsetStateReceiver.onReceive(context, intent);
  }

  private void simulateBluetoothScoConnectionDisconnected() {
    Intent intent = new Intent();
    intent.setAction(BluetoothHeadset.ACTION_AUDIO_STATE_CHANGED);
    intent.putExtra(BluetoothHeadset.EXTRA_STATE, BluetoothHeadset.STATE_AUDIO_DISCONNECTED);
    bluetoothHeadsetStateReceiver.onReceive(context, intent);
  }
}
