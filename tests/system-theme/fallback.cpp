#include <QCoreApplication>
#include <cassert>
#include "systemthemeobserver.h"
int main(int argc, char** argv) {
    QCoreApplication app(argc, argv);
    SystemThemeObserver observer; observer.start();
    app.processEvents();
    assert(observer.preference() == SystemPreference::Unknown);
    ThemePolicy policy; policy.observe(observer.preference());
    assert(policy.selected() == Theme::System && policy.effective() == Theme::Light);
    policy.select(Theme::Dark); assert(policy.effective() == Theme::Dark);
    policy.select(Theme::Light); assert(policy.effective() == Theme::Light);
}
