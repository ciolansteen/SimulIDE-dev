# Theme startup regression

This integration test constructs the real application window with an isolated
configuration and a synthetic dark desktop palette. A saved Light choice must
apply the Fusion light palette on the first startup, not only after a theme
switch. The test also covers saved Dark startup and Dark/Light transitions.

Requirements: the normal Qt5 development dependencies and a C++ compiler.
No network, live desktop, existing profile or circuit is used. Automatic library
update checks are disabled in the temporary profile.

Build outside the source tree:

    mkdir build-theme
    cd build-theme
    qmake /path/to/SimulIDE-dev/tests/theme-startup/theme-startup.pro
    make -j2
    QT_QPA_PLATFORM=offscreen ./theme-startup
    QT_QPA_PLATFORM=offscreen ./theme-startup --dark

Both processes must exit zero. Set THEME_SCREENSHOT to a PNG path to capture the
startup window. The test reports each assertion on standard error. The fixture
links the actual application sources, replacing only the production entry point.
