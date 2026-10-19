#include "themepolicy.h"
#include <cassert>

int main() {
    // normalize tests
    assert(ThemePolicy::normalize(-1) == Theme::System);
    assert(ThemePolicy::normalize(0) == Theme::Light);
    assert(ThemePolicy::normalize(1) == Theme::Dark);
    assert(ThemePolicy::normalize(2) == Theme::System);
    assert(ThemePolicy::normalize(3) == Theme::System);

    // default selected System effective Light (Unknown fallback)
    ThemePolicy p;
    assert(p.selected() == Theme::System);
    assert(p.effective() == Theme::Light);

    // observe Dark then Light keeps selected System
    p.observe(SystemPreference::Dark);
    assert(p.effective() == Theme::Dark);
    p.observe(SystemPreference::Light);
    assert(p.selected() == Theme::System);
    assert(p.effective() == Theme::Light);

    // select Light observe Dark effective Light
    p.select(Theme::Light);
    p.observe(SystemPreference::Dark);
    assert(p.effective() == Theme::Light);

    // select Dark observe Light effective Dark
    p.select(Theme::Dark);
    p.observe(SystemPreference::Light);
    assert(p.effective() == Theme::Dark);

    // return System effective Light (reset state)
    p.select(Theme::System);
    p.observe(SystemPreference::Unknown);
    assert(p.selected() == Theme::System);
    assert(p.effective() == Theme::Light);

    return 0;
}
