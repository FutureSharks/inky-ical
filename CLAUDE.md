# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## What this is

Firmware for a LilyGo T5 e-Paper S3 (ESP32-S3, 960×540 e-ink) that fetches one or more Google/Apple/Microsoft 365 iCal feeds over WiFi, merges events chronologically, renders them to the e-ink display, and deep-sleeps between refreshes. Arduino C++.

## Commands

### Build & flash (device)

```sh
cp src/secrets_template.h src/secrets.h   # first time only; edit WIFI_SSID/PASSWORD/CALENDAR_URLS
arduino-cli compile                        # uses sketch.yaml in repo root for fqbn/libraries/platform
arduino-cli upload --port <device port>
```

PSRAM must be enabled (OPI) — `inky-ical.ino` has a `#error` guard (`BOARD_HAS_PSRAM`) that fails the build otherwise.

### Host-side parser tests (no device needed)

```sh
cd test
make test                    # runs all three test binaries against test/fixtures/sample.ics
make test ICS=/tmp/feed.ics  # run test_readers against a real feed instead
```

Individual binaries: `make test_chunked_decoder`, `make test_line_splitting`, `make test_readers`. These compile the vendored uICAL sources plus (for `test_chunked_decoder`) the firmware's own `src/chunked_decoder.h` directly, so a pass covers the actual shipped code, not a reimplementation. `test_readers` reimplements Arduino's `String`/`Stream` classes on the host to exercise `BufferReader`, `StringReader`, and `StreamReader` parse paths.

Never commit real calendar exports (`*.ics` is gitignored except the synthetic `test/fixtures/sample.ics`) — they contain event titles and attendee emails.

## Architecture

### Startup flow (`inky-ical.ino`)

`setup()` runs once per wake cycle: `display_init()` (splash screen) → `wifi_connect()` → `time_set()` (SNTP) → `fetch_calendars()` → `display_calendar()` → deep sleep until the next daily sync time (`SYNC_HOUR`:`SYNC_MINUTE` in `src/config.h`, local time, default 04:00). Any WiFi/time failure logs a warning and returns without syncing (display keeps showing the last successful render / splash). `DEBUG_MODE` in `src/config.h` disables deep sleep and pauses in `loop()` instead, so `Serial` stays alive for debugging.

### Calendar fetch pipeline (`src/calendar_sync.cpp`)

Each URL is fetched and parsed one at a time (not concurrently) so only one feed's buffer is in memory at once. The critical design constraint, discovered the hard way (see `upstream-pr/ISSUE.md`), is: **never parse iCal data directly off the network socket.**

- `HTTPClient::getStreamPtr()` on ESP32 returns the raw socket with HTTP/1.1 chunked-transfer framing (`Transfer-Encoding: chunked`) still in place — chunk-size lines like `7ff2\r\n` would otherwise reach the uICAL parser and fail as `ParseError: VLINE does not have a ':'`.
- Even with framing stripped, parsing directly from the socket ties the read rate to parse speed; on real feeds (~213 KB) the TCP receive window fills and the transfer stalls mid-body with the connection still open.
- The fix used here: drain the full response into a PSRAM buffer (`heap_caps_realloc(..., MALLOC_CAP_SPIRAM)`, falling back to `MALLOC_CAP_8BIT`) via `BufferReader` (`src/buffer_reader.h`), *then* parse from that buffer. PSRAM (8 MB) absorbs this; the internal heap (~280 KB) is already committed to WiFi/TLS.
- `src/chunked_decoder.h` is a from-scratch, Arduino-independent chunked-transfer decoder (RFC 9112 §7.1) used while draining — kept free of Arduino types specifically so the host tests exercise the real firmware code.

`collect_events()` wraps the vendored uICAL library (`libraries/uICAL`) to turn a parsed calendar into `CalendarEvent` structs, filtered to `DISPLAY_DAYS` and capped per feed at `MAX_EVENTS_PER_CALENDAR`, then merged across calendars, sorted by `start_epoch` and trimmed to `MAX_EVENTS_TO_DISPLAY` (`src/config.h`).

`CalendarEvent` holds **data, not display text** — `start_epoch`, `end_epoch`, `all_day`, title, calendar name. All wording ("Tomorrow", "at 09:00", title truncation) happens in `src/display_calendar.cpp`, so a layout change never reaches back into the fetch/parse code.

Two transport/memory helpers used by the drain loop live on their own: `src/http_byte_source.h` (blocking block-wise reads off the socket, with the end-of-body detection that TLS makes tricky) and `src/psram_buffer.h` (growable PSRAM buffer).

### Vendored libraries (`libraries/`)

- `uICAL` — vendored locally instead of via the Arduino Library Manager because it carries an unmerged upstream fix (`upstream-pr/0001-*.patch`, tracked at `upstream-pr/ISSUE.md` / `PR.md`) for two bugs in `istream_Stream`: lines split across TCP segment boundaries, and `peek()` breaking line-unfolding during a stall. `sketch.yaml` points at `dir: libraries/uICAL` with the Library Manager version commented out; switch back once the upstream PR merges.
- `roboto` — bitmap font headers for the e-ink display renderer.

### Display (`src/display*.cpp` / `display.h`)

Public interface is `src/display.h`; the implementation is split three ways, with `src/display_internal.h` holding the shared framebuffer, layout constants and primitives:

- `display.cpp` — framebuffer lifecycle (`display_flush()`, `display_clear_framebuffer()`), the battery widgets, and `display_message_screen()`, the shared scaffolding for full-screen message screens.
- `display_status.cpp` — splash/init screen, the accumulating status lines, and the two terminal screens (low battery, startup failure).
- `display_calendar.cpp` — the date header, event rows, and all event formatting.

All positional numbers and type sizes live in the `layout` namespace in `display_internal.h` — adjust the panel layout there rather than in the drawing code.

`display_init()` shows a splash/init screen that `display_init_status()` / `display_error()` append status lines to; it stays on screen until `display_calendar()` successfully renders and clears it — so a mid-init failure is visible on the device rather than showing a blank/stale screen. It returns `false` if the framebuffer could not be allocated, after which every other display call is a no-op. `DISPLAY_CALENDAR_NAME` in `src/config.h` controls what the layout includes.

### Configuration

- `src/config.h` — daily sync time, display window/count, log level, debug mode. Edit directly (not gitignored). The **local timezone** is not here: it's the POSIX `TZ` rule in `src/setup_time.cpp`.
- `src/secrets.h` — WiFi credentials and `CALENDAR_URLS`; gitignored, generated from `src/secrets_template.h`.
