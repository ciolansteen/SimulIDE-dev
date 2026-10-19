// SPDX-License-Identifier: GPL-3.0-or-later
// Three-mode policy inspired by J. Adly, SimulIDE-dev 33da1a491d47.
#pragma once

#include <cstdint>

enum class Theme : std::uint8_t { Light = 0, Dark = 1, System = 2 };
enum class SystemPreference : std::uint8_t { Unknown, Light, Dark };

class ThemePolicy {
public:
    static Theme normalize(int value) {
        if (value >= 0 && value <= 2) {
            return static_cast<Theme>(value);
        }
        return Theme::System;
    }

    Theme selected() const { return selected_; }
    void select(Theme theme) { selected_ = theme; }
    void observe(SystemPreference pref) { preference_ = pref; }

    Theme effective() const {
        if (selected_ != Theme::System) {
            return selected_;
        }
        return (preference_ == SystemPreference::Dark) ? Theme::Dark : Theme::Light;
    }

private:
    Theme selected_ = Theme::System;
    SystemPreference preference_ = SystemPreference::Unknown;
};
