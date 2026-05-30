// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/**
 * @fileoverview Compatibility barrel for New Tab Page lazy-loaded features.
 * Runtime code should import the smaller feature bundles directly.
 */

export {CustomizeButtonsElement} from 'chrome://new-tab-page/shared/customize_buttons/customize_buttons.js';
export * from './lazy_action_chips.js';
export * from './lazy_composebox.js';
export * from './lazy_lens.js';
export * from './lazy_modules.js';
export * from './lazy_promos.js';
export * from './lazy_voice.js';
