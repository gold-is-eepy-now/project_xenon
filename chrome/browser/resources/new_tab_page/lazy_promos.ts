// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Lazy-loaded New Tab Page promo dependencies. */

import './middle_slot_promo.js';
import './ntp_promo/individual_promos.js';
import './ntp_promo/ntp_promo_proxy.js';

export {MiddleSlotPromoElement, PromoDismissAction} from './middle_slot_promo.js';
export {IndividualPromosElement} from './ntp_promo/individual_promos.js';
export {NtpPromoProxyImpl} from './ntp_promo/ntp_promo_proxy.js';
export type {NtpPromoProxy} from './ntp_promo/ntp_promo_proxy.js';
