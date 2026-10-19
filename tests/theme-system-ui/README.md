# Follow system appearance

The App settings selector offers Light, Dark and Follow system. Changes apply immediately, without reloading the circuit. Follow system tracks the light/dark preference; it does not clone a KDE/GTK theme, accent or arbitrary stylesheet.

## Settings compatibility

The `theme` setting stores the selected policy: `0` is Light, `1` is Dark and `2` is Follow system. Existing explicit choices remain explicit. New profiles and malformed/out-of-range settings select Follow system. Changing the effective scheme never replaces the saved policy.

## Linux Qt5 backend

When QtDBus is available at build time, the observer uses the XDG Settings portal at `org.freedesktop.portal.Desktop`, `/org/freedesktop/portal/desktop`, interface `org.freedesktop.portal.Settings`, namespace `org.freedesktop.appearance`, key `color-scheme`.

Reads are asynchronous. `ReadOne` is attempted first, with `Read` compatibility for older portals. The observer handles nested variants, service owner changes and stale initial replies. Notifications are subscribed before reading. It never samples the application's palette and has no periodic polling timer.

An explicit portal preference resolves to the corresponding built-in scheme. No preference or an unavailable backend at startup resolves to Light immediately. A service outage after a valid observation retains that observation until a new value arrives. Manual Light/Dark choices ignore system notifications, while observations are retained for returning to Follow system.

QtDBus is optional. Builds without it (or configured with `CONFIG+=no_system_theme_dbus`) retain manual modes and the deterministic Light fallback for Follow system.

## Platform and verification limits

The deterministic Linux protocol tests use a private session bus, not the user's desktop portal. Native KDE/GNOME session changes, forced-color/high-contrast accessibility behavior and complete desktop-theme parity require separate native-session verification. Windows/macOS native appearance observation is not implemented by this change; Follow system uses the fallback there. Do not interpret a passing Linux test as universal native-platform support.

The existing settings dialog contains unrelated sizing constraints. Tests check the Follow system selector's size specifically; no whole-dialog layout redesign is included.

## Tests

Build each qmake project in a separate directory unless intentionally reusing full-application object files. No tests change the global desktop theme. The GUI fixtures use temporary HOME/XDG profiles, synthetic circuits and disabled update checks.

    qmake /path/to/source/tests/theme-policy/theme-policy.pro
    make
    ./theme-policy

    qmake /path/to/source/tests/system-theme/system-theme.pro
    make
    dbus-run-session -- ./system-theme

    qmake /path/to/source/tests/system-theme/fallback.pro
    make
    ./system-theme-fallback

    qmake /path/to/source/tests/theme-startup/theme-startup.pro
    make
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-startup
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-startup --dark
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-startup --system
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-startup --missing-theme
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-startup --malformed-theme
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-startup --out-of-range-theme

    qmake /path/to/source/tests/theme-system-ui/theme-system-ui.pro
    make
    dbus-run-session -- env QT_QPA_PLATFORM=offscreen ./theme-system-ui

`theme-system-ui --injected` exercises policy/UI with direct observations instead of portal delivery; it must not be reported as native-backend evidence. The default test uses actual D-Bus calls/signals against a controlled portal. It drives the real settings combo, verifies persistence in a fresh process, manual overrides and delayed reads, and checks semantic color-pointer stability, circuit serialization/selection, editor text/undo/modified state and running simulation state. Optional `THEME_CAPTURE_DIR` saves rendered application and settings/property dialog captures.

The timeout fixture serves the portal on a different D-Bus connection. Qt's same-connection local-call optimization does not exercise the normal transport timeout path.

## Attribution and scope

The three-mode selection and effective-change notification concepts were informed by J. Adly (`youssefadly237`), commit `33da1a491d473c41115d11d840f0111b8e0f2808` on `feature/dark-light-theme-support`:

https://github.com/youssefadly237/SimulIDE-dev/commit/33da1a491d473c41115d11d840f0111b8e0f2808

The implementation retains the newer upstream ThemeManager, built-in palettes, semantic color storage and icon/editor refresh paths. The candidate's old QSS/icon redesign and separately hosted theme bundles are not copied. Existing project copyright notices remain intact; new contribution files are GPL-3.0-or-later according to the repository's contribution licensing policy.
