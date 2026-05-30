// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

import 'chrome://resources/cr_elements/cr_button/cr_button.js';

import {CrLitElement} from 'chrome://resources/lit/v3_0/lit.rollup.js';

import type {StartPageLayout, StartPageWidget} from './new_tab_page.mojom-webui.js';
import {StartPageWidgetType} from './new_tab_page.mojom-webui.js';
import {NewTabPageProxy} from './new_tab_page_proxy.js';
import {getCss} from './start_page_editor.css.js';
import {getHtml} from './start_page_editor.html.js';

const WIDGET_LABELS = new Map<StartPageWidgetType, string>([
  [StartPageWidgetType.kSearchBox, 'Search box'],
  [StartPageWidgetType.kCustomLinkGroup, 'Custom link group'],
  [StartPageWidgetType.kBookmarksFolder, 'Bookmarks folder'],
  [StartPageWidgetType.kLocalNote, 'Local note'],
  [StartPageWidgetType.kClockWeather, 'Clock/weather'],
  [StartPageWidgetType.kBlankSpacer, 'Blank spacer'],
  [StartPageWidgetType.kMostVisited, 'Most visited'],
  [StartPageWidgetType.kModules, 'Modules'],
  [StartPageWidgetType.kTextCard, 'Text card'],
]);

const DEFAULT_COLUMNS = 12;

function makeWidget(type: StartPageWidgetType, index: number): StartPageWidget {
  return {
    id: `local-${Date.now()}-${index}`,
    type,
    column: 0,
    row: index * 2,
    width: type === StartPageWidgetType.kBlankSpacer ? 4 : DEFAULT_COLUMNS,
    height: type === StartPageWidgetType.kLocalNote ||
            type === StartPageWidgetType.kTextCard ?
        3 :
        2,
    title: WIDGET_LABELS.get(type) || 'Widget',
    text: '',
    sourceId: '',
    links: [],
  };
}

export class StartPageEditorElement extends CrLitElement {
  static get is() {
    return 'ntp-start-page-editor';
  }

  static override get styles() {
    return getCss();
  }

  override render() {
    return getHtml.bind(this)();
  }

  static override get properties() {
    return {
      layout: {type: Object},
      status_: {type: String},
      importExportText_: {type: String},
    };
  }

  accessor layout: StartPageLayout|null = null;
  protected accessor status_: string = '';
  protected accessor importExportText_: string = '';
  private draggedWidgetId_: string|null = null;

  override connectedCallback() {
    super.connectedCallback();
    this.loadLayout_();
  }

  protected widgetTypes_(): StartPageWidgetType[] {
    return [
      StartPageWidgetType.kSearchBox,
      StartPageWidgetType.kCustomLinkGroup,
      StartPageWidgetType.kBookmarksFolder,
      StartPageWidgetType.kLocalNote,
      StartPageWidgetType.kClockWeather,
      StartPageWidgetType.kBlankSpacer,
      StartPageWidgetType.kMostVisited,
      StartPageWidgetType.kModules,
      StartPageWidgetType.kTextCard,
    ];
  }

  protected widgetLabel_(type: StartPageWidgetType): string {
    return WIDGET_LABELS.get(type) || 'Widget';
  }

  protected widgetStyle_(widget: StartPageWidget): string {
    return `grid-column: ${widget.column + 1} / span ${
        widget.width}; min-height: ${Math.max(widget.height * 44, 44)}px;`;
  }

  protected isTextWidget_(widget: StartPageWidget): boolean {
    return widget.type === StartPageWidgetType.kLocalNote ||
        widget.type === StartPageWidgetType.kTextCard;
  }

  protected isBookmarksFolderWidget_(widget: StartPageWidget): boolean {
    return widget.type === StartPageWidgetType.kBookmarksFolder;
  }

  protected async loadLayout_() {
    this.layout =
        (await NewTabPageProxy.getInstance().handler.getStartPageLayout())
            .layout;
  }

  protected async onSaveClick_() {
    if (!this.layout) {
      return;
    }
    const result =
        await NewTabPageProxy.getInstance().handler.saveStartPageLayout(
            this.layout);
    this.layout = result.sanitizedLayout;
    this.status_ =
        result.success ? 'Saved local profile layout.' : result.error;
  }

  protected async onResetClick_() {
    this.layout =
        (await NewTabPageProxy.getInstance().handler.resetStartPageLayout())
            .layout;
    this.status_ = 'Reset to default layout.';
  }

  protected async onExportClick_() {
    this.importExportText_ =
        (await NewTabPageProxy.getInstance().handler.exportStartPageLayout())
            .json;
    this.status_ = 'Exported sanitized JSON.';
  }

  protected async onImportClick_() {
    const result =
        await NewTabPageProxy.getInstance().handler.importStartPageLayout(
            this.importExportText_);
    this.layout = result.sanitizedLayout;
    this.status_ = result.success ? 'Imported sanitized layout.' : result.error;
  }

  protected onImportExportInput_(event: Event) {
    this.importExportText_ = (event.target as HTMLTextAreaElement).value;
  }

  protected onClockWeatherEnabledChange_(event: Event) {
    if (!this.layout) {
      return;
    }
    this.layout = {
      ...this.layout,
      clockWeatherEnabledLocally: (event.target as HTMLInputElement).checked,
    };
  }

  protected onWidgetStringInput_(event: Event) {
    const target = event.target as HTMLInputElement | HTMLTextAreaElement;
    const id = target.dataset['id'];
    const field =
        target.dataset['field'] as 'title' | 'text' | 'sourceId' | undefined;
    if (!this.layout || !id || !field) {
      return;
    }
    this.layout = {
      ...this.layout,
      widgets: this.layout.widgets.map(
          widget =>
              widget.id === id ? {...widget, [field]: target.value} : widget),
    };
  }

  protected onAddWidgetClick_(event: Event) {
    if (!this.layout) {
      return;
    }
    const type = Number((event.currentTarget as HTMLElement).dataset['type']) as
        StartPageWidgetType;
    this.layout = {
      ...this.layout,
      widgets:
          [...this.layout.widgets, makeWidget(type, this.layout.widgets.length)]
    };
  }

  protected onRemoveWidgetClick_(event: Event) {
    const id = (event.currentTarget as HTMLElement).dataset['id'];
    if (!this.layout || !id) {
      return;
    }
    this.layout = {
      ...this.layout,
      widgets: this.layout.widgets.filter(widget => widget.id !== id)
    };
  }

  protected onDragStart_(event: DragEvent) {
    this.draggedWidgetId_ =
        (event.currentTarget as HTMLElement).dataset['id'] || null;
    event.dataTransfer?.setData('text/plain', this.draggedWidgetId_ || '');
  }

  protected onDragOver_(event: DragEvent) {
    event.preventDefault();
  }

  protected onDrop_(event: DragEvent) {
    event.preventDefault();
    const targetId = (event.currentTarget as HTMLElement).dataset['id'];
    const draggedId =
        this.draggedWidgetId_ || event.dataTransfer?.getData('text/plain');
    if (!this.layout || !targetId || !draggedId || targetId === draggedId) {
      return;
    }
    const widgets = [...this.layout.widgets];
    const from = widgets.findIndex(widget => widget.id === draggedId);
    const to = widgets.findIndex(widget => widget.id === targetId);
    if (from === -1 || to === -1) {
      return;
    }
    const [moved] = widgets.splice(from, 1);
    widgets.splice(to, 0, moved);
    widgets.forEach((widget, index) => widget.row = index * 2);
    this.layout = {...this.layout, widgets};
  }

  protected resizeWidget_(event: Event, delta: number) {
    const id = (event.currentTarget as HTMLElement).dataset['id'];
    if (!this.layout || !id) {
      return;
    }
    this.layout = {
      ...this.layout,
      widgets: this.layout.widgets.map(
          widget => widget.id === id ? {
            ...widget,
            width: Math.min(DEFAULT_COLUMNS, Math.max(1, widget.width + delta))
          } :
                                       widget),
    };
  }
}

declare global {
  interface HTMLElementTagNameMap {
    'ntp-start-page-editor': StartPageEditorElement;
  }
}

customElements.define(StartPageEditorElement.is, StartPageEditorElement);
