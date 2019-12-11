// Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

/**
 * Opens the score stats inspector dialog.
 * @param {String} dialogId: identifier of the dialog to show.
 * @return {DOMElement} The dialog element that has been opened.
 */
function openScoreStatsInspector(dialogId) {
  var dialog = document.getElementById(dialogId);
  dialog.showModal();
  return dialog;
}

/**
 * Closes the score stats inspector dialog.
 */
function closeScoreStatsInspector() {
  var dialog = document.querySelector('dialog[open]');
  if (dialog == null)
    return;
  dialog.close();
}

/**
 * Audio inspector class.
 * @constructor
 */
function AudioInspector() {
  console.debug('Creating an AudioInspector instance.');
  this.audioPlayer_ = new Audio();
  this.metadata_ = {};
  this.currentScore_ = null;
  this.audioInspector_ = null;
  this.snackbarContainer_ = document.querySelector('#snackbar');

  // Get base URL without anchors.
  this.baseUrl_ = window.location.href;
  var index = this.baseUrl_.indexOf('#');
  if (index > 0)
    this.baseUrl_ = this.baseUrl_.substr(0, index)
  console.info('Base URL set to "' + window.location.href + '".');

  window.event.stopPropagation();
  this.createTextAreasForCopy_();
  this.createAudioInspector_();
  this.initializeEventHandlers_();

  // When MDL is ready, parse the anchor (if any) to show the requested
  // experiment.
  var self = this;
  document.querySelectorAll('header a')[0].addEventListener(
      'mdl-componentupgraded', function() {
    if (!self.parseWindowAnchor()) {
      // If not experiment is requested, open the first section.
      console.info('No anchor parsing, opening the first section.');
      document.querySelectorAll('header a > span')[0].click();
    }
  });
}

/**
 * Parse the anchor in the window URL.
 * @return {bool} True if the parsing succeeded.
 */
AudioInspector.prototype.parseWindowAnchor = function() {
  var index = location.href.indexOf('#');
  if (index == -1) {
    console.debug('No # found in the URL.');
    return false;
  }

  var anchor = location.href.substr(index - location.href.length + 1);
  console.info('Anchor changed: "' + anchor + '".');

  var parts = anchor.split('&');
  if (parts.length != 3) {
    console.info('Ignoring anchor with invalid number of fields.');
    return false;
  }

  var openDialog = document.querySelector('dialog[open]');
  try {
    // Open the requested dialog if not already open.
    if (!openDialog || openDialog.id != parts[1]) {
      !openDialog || openDialog.close();
      document.querySelectorAll('header a > span')[
          parseInt(parts[0].substr(1))].click();
      openDialog = openScoreStatsInspector(parts[1]);
    }

    // Trigger click on cell.
    var cell = openDialog.querySelector('td.' + parts[2]);
    cell.focus();
    cell.click();

    this.showNotification_('Experiment selected.');
    return true;
  } catch (e) {
    this.showNotification_('Cannot select experiment :(');
    console.error('Exception caught while selecting experiment: "' + e + '".');
  }

  return false;
}

/**
 * Set up the inspector for a new score.
 * @param {DOMElement} element: Element linked to the selected score.
 */
AudioInspector.prototype.selectedScoreChange = function(element) {
  if (this.currentScore_ == element) { return; }
  if (this.currentScore_ != null) {
    this.currentScore_.classList.remove('selected-score');
  }
  this.currentScore_ = element;
  this.currentScore_.classList.add('selected-score');
  this.stopAudio();

  // Read metadata.
  var matches = element.querySelectorAll('input[type=hidden]');
  this.metadata_ = {};
  for (var index = 0; index < matches.length; ++index) {
    this.metadata_[matches[index].name] = matches[index].value;
  }

  // Show the audio inspector interface.
  var container = element.parentNode.parentNode.parentNode.parentNode;
  var audioInspectorPlaceholder = container.querySelector(
      '.audio-inspector-placeholder');
  this.moveInspector_(audioInspectorPlaceholder);
};

/**
 * Stop playing audio.
 */
AudioInspector.prototype.stopAudio = function() {
  console.info('Pausing audio play out.');
  this.audioPlayer_.pause();
};

/**
 * Show a text message using the snackbar.
 */
AudioInspector.prototype.showNotification_ = function(text) {
  try {
    this.snackbarContainer_.MaterialSnackbar.showSnackbar({
        message: text, timeout: 2000});
  } catch (e) {
    // Fallback to an alert.
    alert(text);
    console.warn('Cannot use snackbar: "' + e + '"');
  }
}

/**
 * Move the audio inspector DOM node into the given parent.
 * @param {DOMElement} newParentNode: New parent for the inspector.
 */
AudioInspector.prototype.moveInspector_ = function(newParentNode) {
  newParentNode.appendChild(this.audioInspector_);
};

/**
 * Play audio file from url.
 * @param {string} metadataFieldName: Metadata field name.
 */
AudioInspector.prototype.playAudio = function(metadataFieldName) {
  if (this.metadata_[metadataFieldName] == undefined) { return; }
  if (this.metadata_[metadataFieldName] == 'None') {
    alert('The selected stream was not used during the experiment.');
    return;
  }
  this.stopAudio();
  this.audioPlayer_.src = this.metadata_[metadataFieldName];
  console.debug('Audio source URL: "' + this.audioPlayer_.src + '"');
  this.audioPlayer_.play();
  console.info('Playing out audio.');
};

/**
 * Create hidden text areas to copy URLs.
 *
 * For each dialog, one text area is created since it is not possible to select
 * text on a text area outside of the active dialog.
 */
AudioInspector.prototype.createTextAreasForCopy_ = function() {
  var self = this;
  document.querySelectorAll('dialog.mdl-dialog').forEach(function(element) {
    var textArea = document.createElement("textarea");
    textArea.classList.add('url-copy');
    textArea.style.position = 'fixed';
    textArea.style.bottom = 0;
    textArea.style.left = 0;
    textArea.style.width = '2em';
    textArea.style.height = '2em';
    textArea.style.border = 'none';
    textArea.style.outline = 'none';
    textArea.style.boxShadow = 'none';
    textArea.style.background = 'transparent';
    textArea.style.fontSize = '6px';
    element.appendChild(textArea);
  });
}

/**
 * Create audio inspector.
 */
AudioInspector.prototype.createAudioInspector_ = function() {
  var buttonIndex = 0;
  function getButtonHtml(icon, toolTipText, caption, metadataFieldName) {
    var buttonId = 'audioInspectorButton' + buttonIndex++;
    html = caption == null ? '' : caption;
    html += '<button class="mdl-button mdl-js-button mdl-button--icon ' +
                'mdl-js-ripple-effect" id="' + buttonId + '">' +
              '<i class="material-icons">' + icon + '</i>' +
              '<div class="mdl-tooltip" data-mdl-for="' + buttonId + '">' +
                 toolTipText +
              '</div>';
    if (metadataFieldName != null) {
      html += '<input type="hidden" value="' + metadataFieldName + '">'
    }
    html += '</button>'

    return html;
  }

  // TODO(alessiob): Add timeline and highlight current track by changing icon
  // color.

  this.audioInspector_ = document.createElement('div');
  this.audioInspector_.classList.add('audio-inspector');
  this.audioInspector_.innerHTML =
      '<div class="mdl-grid">' +
        '<div class="mdl-layout-spacer"></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Simulated echo', 'E<sub>in</sub>',
                        'echo_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('stop', 'Stop playing [S]', null, '__stop__') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Render stream', 'R<sub>in</sub>',
                        'render_filepath') +
        '</div>' +
        '<div class="mdl-layout-spacer"></div>' +
      '</div>' +
      '<div class="mdl-grid">' +
        '<div class="mdl-layout-spacer"></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Capture stream (APM input) [1]',
                        'Y\'<sub>in</sub>', 'capture_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col"><strong>APM</strong></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'APM output [2]', 'Y<sub>out</sub>',
                        'apm_output_filepath') +
        '</div>' +
        '<div class="mdl-layout-spacer"></div>' +
      '</div>' +
      '<div class="mdl-grid">' +
        '<div class="mdl-layout-spacer"></div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Echo-free capture stream',
                        'Y<sub>in</sub>', 'echo_free_capture_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'Clean capture stream',
                        'Y<sub>clean</sub>', 'clean_capture_input_filepath') +
        '</div>' +
        '<div class="mdl-cell mdl-cell--2-col">' +
          getButtonHtml('play_arrow', 'APM reference [3]', 'Y<sub>ref</sub>',
                        'apm_reference_filepath') +
        '</div>' +
        '<div class="mdl-layout-spacer"></div>' +
      '</div>';

  // Add an invisible node as initial container for the audio inspector.
  var parent = document.createElement('div');
  parent.style.display = 'none';
  this.moveInspector_(parent);
  document.body.appendChild(parent);
};

/**
 * Initialize event handlers.
 */
AudioInspector.prototype.initializeEventHandlers_ = function() {
  var self = this;

  // Score cells.
  document.querySelectorAll('td.single-score-cell').forEach(function(element) {
    element.onclick = function() {
      self.selectedScoreChange(this);
    }
  });

  // Copy anchor URLs icons.
  if (document.queryCommandSupported('copy')) {
    document.querySelectorAll('td.single-score-cell button').forEach(
        function(element) {
      element.onclick = function() {
        // Find the text area in the dialog.
        var textArea = element.closest('dialog').querySelector(
            'textarea.url-copy');

        // Copy.
        textArea.value = self.baseUrl_ + '#' + element.getAttribute(
            'data-anchor');
        textArea.select();
        try {
          if (!document.execCommand('copy'))
            throw 'Copy returned false';
          self.showNotification_('Experiment URL copied.');
        } catch (e) {
          self.showNotification_('Cannot copy experiment URL :(');
          console.error(e);
        }
      }
    });
  } else {
    self.showNotification_(
        'The copy command is disabled. URL copy is not enabled.');
  }

  // Audio inspector buttons.
  this.audioInspector_.querySelectorAll('button').forEach(function(element) {
    var target = element.querySelector('input[type=hidden]');
    if (target == null) { return; }
    element.onclick = function() {
      if (target.value == '__stop__') {
        self.stopAudio();
      } else {
        self.playAudio(target.value);
      }
    };
  });

  // Dialog close handlers.
  var dialogs = document.querySelectorAll('dialog').forEach(function(element) {
    element.onclose = function() {
      self.stopAudio();
    }
  });

  // Keyboard shortcuts.
  window.onkeyup = function(e) {
    var key = e.keyCode ? e.keyCode : e.which;
    switch (key) {
      case 49:  // 1.
        self.playAudio('capture_filepath');
        break;
      case 50:  // 2.
        self.playAudio('apm_output_filepath');
        break;
      case 51:  // 3.
        self.playAudio('apm_reference_filepath');
        break;
      case 83:  // S.
      case 115:  // s.
        self.stopAudio();
        break;
    }
  };

  // Hash change.
  window.onhashchange = function(e) {
    self.parseWindowAnchor();
  }
};
