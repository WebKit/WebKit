/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidvoip;

import android.Manifest.permission;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.pm.PackageManager;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;
import org.webrtc.ContextUtils;

public class MainActivity extends Activity implements OnVoipClientTaskCompleted {
  private static final int NUM_SUPPORTED_CODECS = 6;

  private VoipClient voipClient;
  private List<String> supportedCodecs;
  private boolean[] isDecoderSelected;
  private Set<Integer> selectedDecoders;

  private Toast toast;
  private ScrollView scrollView;
  private TextView localIPAddressTextView;
  private EditText localPortNumberEditText;
  private EditText remoteIPAddressEditText;
  private EditText remotePortNumberEditText;
  private Spinner encoderSpinner;
  private Button decoderSelectionButton;
  private TextView decodersTextView;
  private ToggleButton sessionButton;
  private RelativeLayout switchLayout;
  private Switch sendSwitch;
  private Switch playoutSwitch;

  @Override
  protected void onCreate(Bundle savedInstance) {
    ContextUtils.initialize(getApplicationContext());

    super.onCreate(savedInstance);
    setContentView(R.layout.activity_main);

    System.loadLibrary("examples_androidvoip_jni");

    voipClient = new VoipClient(getApplicationContext(), this);
    voipClient.getAndSetUpLocalIPAddress();
    voipClient.getAndSetUpSupportedCodecs();

    isDecoderSelected = new boolean[NUM_SUPPORTED_CODECS];
    selectedDecoders = new HashSet<>();

    toast = Toast.makeText(this, "", Toast.LENGTH_SHORT);

    scrollView = (ScrollView) findViewById(R.id.scroll_view);
    localIPAddressTextView = (TextView) findViewById(R.id.local_ip_address_text_view);
    localPortNumberEditText = (EditText) findViewById(R.id.local_port_number_edit_text);
    remoteIPAddressEditText = (EditText) findViewById(R.id.remote_ip_address_edit_text);
    remotePortNumberEditText = (EditText) findViewById(R.id.remote_port_number_edit_text);
    encoderSpinner = (Spinner) findViewById(R.id.encoder_spinner);
    decoderSelectionButton = (Button) findViewById(R.id.decoder_selection_button);
    decodersTextView = (TextView) findViewById(R.id.decoders_text_view);
    sessionButton = (ToggleButton) findViewById(R.id.session_button);
    switchLayout = (RelativeLayout) findViewById(R.id.switch_layout);
    sendSwitch = (Switch) findViewById(R.id.start_send_switch);
    playoutSwitch = (Switch) findViewById(R.id.start_playout_switch);

    setUpSessionButton();
    setUpSendAndPlayoutSwitch();
  }

  private void setUpEncoderSpinner(List<String> supportedCodecs) {
    ArrayAdapter<String> encoderAdapter =
        new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, supportedCodecs);
    encoderAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
    encoderSpinner.setAdapter(encoderAdapter);
    encoderSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        voipClient.setEncoder((String) parent.getSelectedItem());
      }
      @Override
      public void onNothingSelected(AdapterView<?> parent) {}
    });
  }

  private List<String> getSelectedDecoders() {
    List<String> decoders = new ArrayList<>();
    for (int i = 0; i < supportedCodecs.size(); i++) {
      if (selectedDecoders.contains(i)) {
        decoders.add(supportedCodecs.get(i));
      }
    }
    return decoders;
  }

  private void setUpDecoderSelectionButton(List<String> supportedCodecs) {
    decoderSelectionButton.setOnClickListener((view) -> {
      AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
      dialogBuilder.setTitle(R.string.dialog_title);

      // Populate multi choice items with supported decoders.
      String[] supportedCodecsArray = supportedCodecs.toArray(new String[0]);
      dialogBuilder.setMultiChoiceItems(
          supportedCodecsArray, isDecoderSelected, (dialog, position, isChecked) -> {
            if (isChecked) {
              selectedDecoders.add(position);
            } else if (!isChecked) {
              selectedDecoders.remove(position);
            }
          });

      // "Ok" button.
      dialogBuilder.setPositiveButton(R.string.ok_label, (dialog, position) -> {
        List<String> decoders = getSelectedDecoders();
        String result = decoders.stream().collect(Collectors.joining(", "));
        if (result.isEmpty()) {
          decodersTextView.setText(R.string.decoders_text_view_default);
        } else {
          decodersTextView.setText(result);
        }
        voipClient.setDecoders(decoders);
      });

      // "Dismiss" button.
      dialogBuilder.setNegativeButton(
          R.string.dismiss_label, (dialog, position) -> { dialog.dismiss(); });

      // "Clear All" button.
      dialogBuilder.setNeutralButton(R.string.clear_all_label, (dialog, position) -> {
        Arrays.fill(isDecoderSelected, false);
        selectedDecoders.clear();
        decodersTextView.setText(R.string.decoders_text_view_default);
      });

      AlertDialog dialog = dialogBuilder.create();
      dialog.show();
    });
  }

  private void setUpSessionButton() {
    sessionButton.setOnCheckedChangeListener((button, isChecked) -> {
      // Ask for permission on RECORD_AUDIO if not granted.
      if (ContextCompat.checkSelfPermission(this, permission.RECORD_AUDIO)
          != PackageManager.PERMISSION_GRANTED) {
        String[] sList = {permission.RECORD_AUDIO};
        ActivityCompat.requestPermissions(this, sList, 1);
      }

      if (isChecked) {
        // Order matters here, addresses have to be set before starting session
        // before setting codec.
        voipClient.setLocalAddress(localIPAddressTextView.getText().toString(),
            Integer.parseInt(localPortNumberEditText.getText().toString()));
        voipClient.setRemoteAddress(remoteIPAddressEditText.getText().toString(),
            Integer.parseInt(remotePortNumberEditText.getText().toString()));
        voipClient.startSession();
        voipClient.setEncoder((String) encoderSpinner.getSelectedItem());
        voipClient.setDecoders(getSelectedDecoders());
      } else {
        voipClient.stopSession();
      }
    });
  }

  private void setUpSendAndPlayoutSwitch() {
    sendSwitch.setOnCheckedChangeListener((button, isChecked) -> {
      if (isChecked) {
        voipClient.startSend();
      } else {
        voipClient.stopSend();
      }
    });

    playoutSwitch.setOnCheckedChangeListener((button, isChecked) -> {
      if (isChecked) {
        voipClient.startPlayout();
      } else {
        voipClient.stopPlayout();
      }
    });
  }

  private void setUpIPAddressEditTexts(String localIPAddress) {
    if (localIPAddress.isEmpty()) {
      showToast("Please check your network configuration");
    } else {
      localIPAddressTextView.setText(localIPAddress);
      // By default remote IP address is the same as local IP address.
      remoteIPAddressEditText.setText(localIPAddress);
    }
  }

  private void showToast(String message) {
    if (toast != null) {
      toast.cancel();
      toast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
      toast.setGravity(Gravity.TOP, 0, 200);
      toast.show();
    }
  }

  @Override
  protected void onDestroy() {
    voipClient.close();
    voipClient = null;

    super.onDestroy();
  }

  @Override
  public void onGetLocalIPAddressCompleted(String localIPAddress) {
    runOnUiThread(() -> { setUpIPAddressEditTexts(localIPAddress); });
  }

  @Override
  public void onGetSupportedCodecsCompleted(List<String> supportedCodecs) {
    runOnUiThread(() -> {
      this.supportedCodecs = supportedCodecs;
      setUpEncoderSpinner(supportedCodecs);
      setUpDecoderSelectionButton(supportedCodecs);
    });
  }

  @Override
  public void onVoipClientInitializationCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (!isSuccessful) {
        showToast("Error initializing audio device");
      }
    });
  }

  @Override
  public void onStartSessionCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (isSuccessful) {
        showToast("Session started");
        switchLayout.setVisibility(View.VISIBLE);
        scrollView.post(() -> { scrollView.fullScroll(ScrollView.FOCUS_DOWN); });
      } else {
        showToast("Failed to start session");
      }
    });
  }

  @Override
  public void onStopSessionCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (isSuccessful) {
        showToast("Session stopped");
        // Set listeners to null so the checked state can be changed programmatically.
        sendSwitch.setOnCheckedChangeListener(null);
        playoutSwitch.setOnCheckedChangeListener(null);
        sendSwitch.setChecked(false);
        playoutSwitch.setChecked(false);
        // Redo the switch listener setup.
        setUpSendAndPlayoutSwitch();
        switchLayout.setVisibility(View.GONE);
      } else {
        showToast("Failed to stop session");
      }
    });
  }

  @Override
  public void onStartSendCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (isSuccessful) {
        showToast("Started sending");
      } else {
        showToast("Error initializing microphone");
      }
    });
  }

  @Override
  public void onStopSendCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (isSuccessful) {
        showToast("Stopped sending");
      } else {
        showToast("Microphone termination failed");
      }
    });
  }

  @Override
  public void onStartPlayoutCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (isSuccessful) {
        showToast("Started playout");
      } else {
        showToast("Error initializing speaker");
      }
    });
  }

  @Override
  public void onStopPlayoutCompleted(boolean isSuccessful) {
    runOnUiThread(() -> {
      if (isSuccessful) {
        showToast("Stopped playout");
      } else {
        showToast("Speaker termination failed");
      }
    });
  }

  @Override
  public void onUninitializedVoipClient() {
    runOnUiThread(() -> { showToast("Voip client is uninitialized"); });
  }
}
