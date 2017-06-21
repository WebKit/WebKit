// Copyright (c) 2017 The WebRTC project authors. All Rights Reserved.
//
// Use of this source code is governed by a BSD-style license
// that can be found in the LICENSE file in the root of the source
// tree. An additional intellectual property rights grant can be found
// in the file PATENTS.  All contributing project authors may
// be found in the AUTHORS file in the root of the source tree.

/**
 * Inspector UI class.
 * @constructor
 */
function Inspector() {
  this.audioPlayer_ = new Audio();
  this.inspectorNode_ = document.createElement('div');
  this.divTestDataGeneratorName_ = document.createElement('div');
  this.divTestDataGenParameters_ = document.createElement('div');
  this.buttonPlayAudioIn_ = document.createElement('button');
  this.buttonPlayAudioOut_ = document.createElement('button');
  this.buttonPlayAudioRef_ = document.createElement('button');
  this.buttonStopAudio_ = document.createElement('button');

  this.selectedItem_ = null;
  this.audioInUrl_ = null;
  this.audioOutUrl_ = null;
  this.audioRefUrl_ = null;
}

/**
 * Initialize.
 */
Inspector.prototype.init = function() {
  window.event.stopPropagation();

  // Create inspector UI.
  this.buildInspector_();
  var body = document.getElementsByTagName('body')[0];
  body.appendChild(this.inspectorNode_);

  // Bind click handler.
  var self = this;
  var items = document.getElementsByClassName('score');
  for (var index = 0; index < items.length; index++) {
    items[index].onclick = function() {
      self.openInspector(this);
    };
  }

  // Bind pressed key handlers.
  var self = this;
  window.onkeyup = function(e) {
    var key = e.keyCode ? e.keyCode : e.which;
    switch (key) {
      case 49:  // 1.
        self.playAudioIn();
        break;
      case 50:  // 2.
        self.playAudioOut();
        break;
      case 51:  // 3.
        self.playAudioRef();
        break;
      case 83:  // S.
      case 115:  // s.
        self.stopAudio();
        break;
    }
  };
};

/**
 * Open the inspector.
 * @param {DOMElement} target: score element that has been clicked.
 */
Inspector.prototype.openInspector = function(target) {
  if (this.selectedItem_ != null) {
    this.selectedItem_.classList.remove('selected');
  }
  this.selectedItem_ = target;
  this.selectedItem_.classList.add('selected');

  var target = this.selectedItem_.querySelector('.test-data-gen-desc');
  var testDataGenName = target.querySelector('input[name=gen_name]').value;
  var testDataGenParams = target.querySelector('input[name=gen_params]').value;
  var audioIn = target.querySelector('input[name=audio_in]').value;
  var audioOut = target.querySelector('input[name=audio_out]').value;
  var audioRef = target.querySelector('input[name=audio_ref]').value;

  this.divTestDataGeneratorName_.innerHTML = testDataGenName;
  this.divTestDataGenParameters_.innerHTML = testDataGenParams;

  this.audioInUrl_ = audioIn;
  this.audioOutUrl_ = audioOut;
  this.audioRefUrl_ = audioRef;
};

/**
 * Play APM audio input signal.
 */
Inspector.prototype.playAudioIn = function() {
  this.play_(this.audioInUrl_);
};

/**
 * Play APM audio output signal.
 */
Inspector.prototype.playAudioOut = function() {
  this.play_(this.audioOutUrl_);
};

/**
 * Play APM audio reference signal.
 */
Inspector.prototype.playAudioRef = function() {
  this.play_(this.audioRefUrl_);
};

/**
 * Stop playing audio.
 */
Inspector.prototype.stopAudio = function() {
  this.audioPlayer_.pause();
};

/**
 * Play audio file from url.
 * @param {string} url
 */
Inspector.prototype.play_ = function(url) {
  if (url == null) {
    alert('Select a score first.');
    return;
  }

  this.audioPlayer_.src = url;
  this.audioPlayer_.play();
};

/**
 * Build inspector.
 */
Inspector.prototype.buildInspector_ = function() {
  var self = this;

  this.inspectorNode_.setAttribute('class', 'inspector');
  this.inspectorNode_.innerHTML =
      '<div class="property test-data-gen-name">' +
         '<div class="name">test data generator</div>' +
      '</div>' +
      '<div class="property test-data-gen-parmas">' +
         '<div class="name">parameters</div>' +
      '</div>' +
      '<div class="buttons"></div>';

  // Add value nodes.
  function addValueNode(node, parent_selector) {
    node.setAttribute('class', 'value');
    node.innerHTML = '-';
    var parentNode = self.inspectorNode_.querySelector(parent_selector);
    parentNode.appendChild(node);
  }
  addValueNode(this.divTestDataGeneratorName_, 'div.test-data-gen-name');
  addValueNode(this.divTestDataGenParameters_, 'div.test-data-gen-parmas');

  // Add buttons.
  var buttonsNode = this.inspectorNode_.querySelector('div.buttons');
  function addButton(node, caption, callback) {
    node.innerHTML = caption;
    buttonsNode.appendChild(node);
    node.onclick = callback.bind(self);
  }
  addButton(this.buttonPlayAudioIn_, 'A_in (<strong>1</strong>)',
            this.playAudioIn);
  addButton(this.buttonPlayAudioOut_, 'A_out (<strong>2</strong>)',
            this.playAudioOut);
  addButton(this.buttonPlayAudioRef_, 'A_ref (<strong>3</strong>)',
            this.playAudioRef);
  addButton(this.buttonStopAudio_, '<strong>S</strong>top', this.stopAudio);
};

/**
 * Instance and initialize the inspector.
 */
function initialize() {
  var inspector = new Inspector();
  inspector.init();
}
