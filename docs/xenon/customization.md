# Xenon customization pillar

Xenon's "more customizable" pillar lets users tailor the browser without
recompiling. Everything is gated behind `xenon_enable_customization_ui`
(declared in [`//build/config/xenon/xenon.gni`](../../build/config/xenon/xenon.gni)),
exposed to C++ as `BUILDFLAG(XENON_ENABLE_CUSTOMIZATION_UI)` via
[`//build/config/xenon:buildflags`](../../build/config/xenon/BUILD.gn).

## Custom start / new tab page

Goal: let users fully customize the page shown when they open a new tab — the
search bar + bookmarks page — with their own content, including a **custom HTML
file** or (later) an in-browser page editor.

### Step 1 — custom URL / local HTML file (implemented)

Rather than loading arbitrary user HTML into Chromium's *privileged*
`chrome://newtab` WebUI (which would expose powerful internal bindings to
untrusted markup — a real security hole), Xenon reuses Chromium's existing,
already-reviewed **new tab page location override** path. That code
(`HandleNewTabPageLocationOverride` in
`chrome/browser/chrome_content_browser_client.cc`) already redirects
`chrome://newtab` to a configured URL; it's how the enterprise
`NewTabPageLocation` policy works.

Xenon adds two user prefs:

| Pref | Type | Meaning |
| --- | --- | --- |
| `xenon.new_tab_page.enabled` (`kXenonNewTabPageEnabled`) | bool | Turn the custom start page on. |
| `xenon.new_tab_page.url` (`kXenonNewTabPageUrl`) | string | The page to load: an `http(s)://` URL **or** a local `file:///path/to/start.html`. |

When enabled and set, a new tab loads that page. The enterprise policy override,
if present, still takes precedence. Incognito is unaffected (matches upstream).

Because the redirect target can be a `file://` URL, "customize with a custom
HTML file" is satisfied today: a user points the pref at their own HTML file.

### Step 2 — in-browser page editor (planned)

A `chrome://settings` surface (gated behind the same buildflag) that:
1. toggles `xenon.new_tab_page.enabled`,
2. provides a simple editor that writes an HTML file into the profile dir and
   sets `xenon.new_tab_page.url` to its `file://` path,
3. offers a few starter templates (search box + bookmarks grid).

The editor is deliberately separated from step 1 so the (small, security-bounded)
redirect mechanism can be reviewed and shipped on its own.

## Security notes

* We intentionally do **not** inject user HTML into a privileged WebUI origin.
  The custom page is loaded as an ordinary web page / local file, with no
  Chrome-internal bindings.
* `file://` new tab pages may be subject to the usual local-file access
  restrictions; this needs confirmation in a real build (see below). `http(s)`
  hosting always works.

## How to verify (needs a real build)

1. Build with `xenon_enable_customization_ui=true`.
2. In a debug build, set the prefs (e.g. via the `chrome://prefs-internals` view
   or a test) — `xenon.new_tab_page.enabled=true`,
   `xenon.new_tab_page.url="https://example.com"` — open a new tab, confirm it
   loads the custom page.
3. Repeat with a `file:///.../start.html` value and confirm the local file
   loads (or document any local-file-access restriction encountered).
4. Confirm that with the buildflag off, the prefs don't exist and behavior is
   identical to upstream.
