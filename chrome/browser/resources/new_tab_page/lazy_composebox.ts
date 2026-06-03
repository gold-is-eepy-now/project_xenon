// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Lazy-loaded composebox dependencies. */

import './ntp_composebox.js';
import 'chrome://resources/cr_components/composebox/composebox.js';
import 'chrome://resources/cr_components/composebox/threads_rail.js';

export type {ComposeboxFile} from 'chrome://resources/cr_components/composebox/common.js';
export {ComposeboxElement, SubmitButtonIconType, VoiceSearchAction} from 'chrome://resources/cr_components/composebox/composebox.js';
export {ComposeboxFileInputsElement} from 'chrome://resources/cr_components/composebox/composebox_file_inputs.js';
export {ComposeboxProxyImpl} from 'chrome://resources/cr_components/composebox/composebox_proxy.js';
export {ErrorScrimElement} from 'chrome://resources/cr_components/composebox/error_scrim.js';
export {ComposeboxFileCarouselElement} from 'chrome://resources/cr_components/composebox/file_carousel.js';
export {ComposeboxFileThumbnailElement} from 'chrome://resources/cr_components/composebox/file_thumbnail.js';
export {ThreadsRailElement} from 'chrome://resources/cr_components/composebox/threads_rail.js';
export {WindowProxy as ComposeboxWindowProxy} from 'chrome://resources/cr_components/composebox/window_proxy.js';
export {PageImageServiceBrowserProxy} from 'chrome://resources/cr_components/page_image_service/browser_proxy.js';
export {NtpComposeboxElement} from './ntp_composebox.js';
