// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

/** @fileoverview Lazy-loaded module dependencies. */

import './modules/module_descriptors.js';
import './modules/modules.js';

export {PluralStringProxyImpl as NTPPluralStringProxyImpl} from 'chrome://resources/js/plural_string_proxy.js';
export {microsoftAuthModuleDescriptor, MicrosoftAuthModuleElement} from './modules/authentication/microsoft_auth_module.js';
export {MicrosoftAuthProxyImpl} from './modules/authentication/microsoft_auth_module_proxy.js';
export {CalendarElement} from './modules/calendar/calendar.js';
export {CalendarEventElement} from './modules/calendar/calendar_event.js';
export {CalendarAction} from './modules/calendar/common.js';
export {googleCalendarDescriptor, GoogleCalendarModuleElement} from './modules/calendar/google_calendar_module.js';
export {GoogleCalendarProxyImpl} from './modules/calendar/google_calendar_proxy.js';
export {outlookCalendarDescriptor, OutlookCalendarModuleElement} from './modules/calendar/outlook_calendar_module.js';
export {OutlookCalendarProxyImpl} from './modules/calendar/outlook_calendar_proxy.js';
// <if expr="not is_official_build">
export {FooProxy} from './modules/dummy/foo_proxy.js';
export {dummyV2Descriptor, ModuleElement as DummyModuleElement} from './modules/dummy/module.js';
// </if>
export {driveModuleDescriptor, DriveModuleElement} from './modules/file_suggestion/drive_module.js';
export {FileProxy} from './modules/file_suggestion/file_module_proxy.js';
export {FileSuggestionElement} from './modules/file_suggestion/file_suggestion.js';
export {microsoftFilesModuleDescriptor, MicrosoftFilesModuleElement} from './modules/file_suggestion/microsoft_files_module.js';
export {MicrosoftFilesProxyImpl} from './modules/file_suggestion/microsoft_files_proxy.js';
export {InfoDialogElement} from './modules/info_dialog.js';
export {ParentTrustedDocumentProxy} from './modules/microsoft_auth_frame_connector.js';
export {ModuleDescriptor} from './modules/module_descriptor.js';
export type {InitializeModuleCallback, Module} from './modules/module_descriptor.js';
export {counterfactualLoad} from './modules/module_descriptors.js';
export {ModuleHeaderElement as ModuleHeaderElementV2} from './modules/module_header.js';
export {ModuleRegistry} from './modules/module_registry.js';
export {ModuleWrapperElement} from './modules/module_wrapper.js';
export type {ModuleInstance} from './modules/module_wrapper.js';
export {ModulesElement, SUPPORTED_MODULE_WIDTHS} from './modules/modules.js';
export type {DisableModuleEvent, DismissModuleElementEvent, DismissModuleInstanceEvent, NamedWidth} from './modules/modules.js';
export {mostRelevantTabResumptionDescriptor, MostRelevantTabResumptionModuleElement} from './modules/most_relevant_tab_resumption/module.js';
export {MostRelevantTabResumptionProxyImpl} from './modules/most_relevant_tab_resumption/most_relevant_tab_resumption_proxy.js';
export {IconContainerElement} from './modules/tab_groups/icon_container.js';
export {COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_DOT_PREFIX, COLOR_NEW_TAB_PAGE_MODULE_TAB_GROUPS_PREFIX, colorIdToString, tabGroupsDescriptor, TabGroupsModuleElement} from './modules/tab_groups/module.js';
export {TabGroupsProxyImpl} from './modules/tab_groups/tab_groups_proxy.js';
