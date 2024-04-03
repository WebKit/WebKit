// Copyright 2023 Google LLC
//
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

package device_specific_configs

import (
	"testing"

	"github.com/stretchr/testify/assert"
)

func TestConfigs_MapKeyMatchesConfigName(t *testing.T) {
	for key, config := range Configs {
		t.Run(config.Name, func(t *testing.T) {
			assert.Equal(t, config.Name, key)
		})
	}
}

func TestConfigs_ConfigsHaveExpectedKeyValuePairs(t *testing.T) {
	for _, config := range Configs {
		t.Run(config.Name, func(t *testing.T) {
			var keys []string
			for key := range config.Keys {
				keys = append(keys, key)
			}
			assert.ElementsMatch(t, []string{"arch", "model", "os"}, keys)
		})
	}
}
