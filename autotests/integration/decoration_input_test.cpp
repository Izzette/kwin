/********************************************************************
KWin - the KDE window manager
This file is part of the KDE project.

Copyright (C) 2016 Martin Gräßlin <mgraesslin@kde.org>

This program is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*********************************************************************/
#include "kwin_wayland_test.h"
#include "platform.h"
#include "abstract_client.h"
#include "cursor.h"
#include "pointer_input.h"
#include "touch_input.h"
#include "screenedge.h"
#include "screens.h"
#include "wayland_server.h"
#include "workspace.h"
#include "shell_client.h"
#include <kwineffects.h>
#include "decorations/decoratedclient.h"

#include <KWayland/Client/connection_thread.h>
#include <KWayland/Client/compositor.h>
#include <KWayland/Client/pointer.h>
#include <KWayland/Client/server_decoration.h>
#include <KWayland/Client/shell.h>
#include <KWayland/Client/seat.h>
#include <KWayland/Client/shm_pool.h>
#include <KWayland/Client/surface.h>

#include <KDecoration2/Decoration>

#include <linux/input.h>

Q_DECLARE_METATYPE(Qt::WindowFrameSection)

namespace KWin
{

static const QString s_socketName = QStringLiteral("wayland_test_kwin_decoration_input-0");

class DecorationInputTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase();
    void init();
    void cleanup();
    void testAxis_data();
    void testAxis();
    void testDoubleClick_data();
    void testDoubleClick();
    void testDoubleTap_data();
    void testDoubleTap();
    void testHover_data();
    void testHover();
    void testPressToMove_data();
    void testPressToMove();
    void testTapToMove_data();
    void testTapToMove();
    void testResizeOutsideWindow_data();
    void testResizeOutsideWindow();

private:
    AbstractClient *showWindow(Test::ShellSurfaceType type);
};

#define MOTION(target) \
    kwinApp()->platform()->pointerMotion(target, timestamp++)

#define PRESS \
    kwinApp()->platform()->pointerButtonPressed(BTN_LEFT, timestamp++)

#define RELEASE \
    kwinApp()->platform()->pointerButtonReleased(BTN_LEFT, timestamp++)

AbstractClient *DecorationInputTest::showWindow(Test::ShellSurfaceType type)
{
    using namespace KWayland::Client;
#define VERIFY(statement) \
    if (!QTest::qVerify((statement), #statement, "", __FILE__, __LINE__))\
        return nullptr;
#define COMPARE(actual, expected) \
    if (!QTest::qCompare(actual, expected, #actual, #expected, __FILE__, __LINE__))\
        return nullptr;

    Surface *surface = Test::createSurface(Test::waylandCompositor());
    VERIFY(surface);
    auto shellSurface = Test::createShellSurface(type, surface, surface);
    VERIFY(shellSurface);
    auto deco = Test::waylandServerSideDecoration()->create(surface, surface);
    QSignalSpy decoSpy(deco, &ServerSideDecoration::modeChanged);
    VERIFY(decoSpy.isValid());
    VERIFY(decoSpy.wait());
    deco->requestMode(ServerSideDecoration::Mode::Server);
    VERIFY(decoSpy.wait());
    COMPARE(deco->mode(), ServerSideDecoration::Mode::Server);
    // let's render
    auto c = Test::renderAndWaitForShown(surface, QSize(500, 50), Qt::blue);
    VERIFY(c);
    COMPARE(workspace()->activeClient(), c);

#undef VERIFY
#undef COMPARE

    return c;
}

void DecorationInputTest::initTestCase()
{
    qRegisterMetaType<KWin::ShellClient*>();
    qRegisterMetaType<KWin::AbstractClient*>();
    QSignalSpy workspaceCreatedSpy(kwinApp(), &Application::workspaceCreated);
    QVERIFY(workspaceCreatedSpy.isValid());
    kwinApp()->platform()->setInitialWindowSize(QSize(1280, 1024));
    QMetaObject::invokeMethod(kwinApp()->platform(), "setOutputCount", Qt::DirectConnection, Q_ARG(int, 2));
    QVERIFY(waylandServer()->init(s_socketName.toLocal8Bit()));

    // change some options
    KSharedConfig::Ptr config = KSharedConfig::openConfig(QString(), KConfig::SimpleConfig);
    config->group(QStringLiteral("MouseBindings")).writeEntry("CommandTitlebarWheel", QStringLiteral("above/below"));
    config->group(QStringLiteral("Windows")).writeEntry("TitlebarDoubleClickCommand", QStringLiteral("OnAllDesktops"));
    config->group(QStringLiteral("Desktops")).writeEntry("Number", 2);
    config->sync();

    kwinApp()->setConfig(config);

    kwinApp()->start();
    QVERIFY(workspaceCreatedSpy.wait());
    QCOMPARE(screens()->count(), 2);
    QCOMPARE(screens()->geometry(0), QRect(0, 0, 1280, 1024));
    QCOMPARE(screens()->geometry(1), QRect(1280, 0, 1280, 1024));
    setenv("QT_QPA_PLATFORM", "wayland", true);
    waylandServer()->initWorkspace();
}

void DecorationInputTest::init()
{
    using namespace KWayland::Client;
    QVERIFY(Test::setupWaylandConnection(Test::AdditionalWaylandInterface::Seat | Test::AdditionalWaylandInterface::Decoration));
    QVERIFY(Test::waitForWaylandPointer());

    screens()->setCurrent(0);
    Cursor::setPos(QPoint(640, 512));
}

void DecorationInputTest::cleanup()
{
    Test::destroyWaylandConnection();
}

void DecorationInputTest::testAxis_data()
{
    QTest::addColumn<QPoint>("decoPoint");
    QTest::addColumn<Qt::WindowFrameSection>("expectedSection");
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("topLeft") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("top") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("topRight") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("topLeft|xdgv5") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("top|xdgv5") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("topRight|xdgv5") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("topLeft|xdgv6") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("top|xdgv6") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("topRight|xdgv6") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::XdgShellV6;
}

void DecorationInputTest::testAxis()
{
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    QCOMPARE(c->titlebarPosition(), AbstractClient::PositionTop);
    QVERIFY(!c->keepAbove());
    QVERIFY(!c->keepBelow());

    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));
    QVERIFY(!input()->pointer()->decoration().isNull());
    QCOMPARE(input()->pointer()->decoration()->decoration()->sectionUnderMouse(), Qt::TitleBarArea);

    // TODO: mouse wheel direction looks wrong to me
    // simulate wheel
    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(c->keepBelow());
    QVERIFY(!c->keepAbove());
    kwinApp()->platform()->pointerAxisVertical(-5.0, timestamp++);
    QVERIFY(!c->keepBelow());
    QVERIFY(!c->keepAbove());
    kwinApp()->platform()->pointerAxisVertical(-5.0, timestamp++);
    QVERIFY(!c->keepBelow());
    QVERIFY(c->keepAbove());

    // test top most deco pixel, BUG: 362860
    c->move(0, 0);
    QFETCH(QPoint, decoPoint);
    MOTION(decoPoint);
    QVERIFY(!input()->pointer()->decoration().isNull());
    QCOMPARE(input()->pointer()->decoration()->client(), c);
    QTEST(input()->pointer()->decoration()->decoration()->sectionUnderMouse(), "expectedSection");
    kwinApp()->platform()->pointerAxisVertical(5.0, timestamp++);
    QVERIFY(!c->keepBelow());
    QVERIFY(!c->keepAbove());
}

void DecorationInputTest::testDoubleClick_data()
{
    QTest::addColumn<QPoint>("decoPoint");
    QTest::addColumn<Qt::WindowFrameSection>("expectedSection");
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("topLeft") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("top") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("topRight") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("topLeft|xdgv5") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("top|xdgv5") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("topRight|xdgv5") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("topLeft|xdgv6") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("top|xdgv6") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("topRight|xdgv6") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::XdgShellV6;
}

void KWin::DecorationInputTest::testDoubleClick()
{
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    QVERIFY(!c->isOnAllDesktops());
    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));

    // double click
    PRESS;
    RELEASE;
    PRESS;
    RELEASE;
    QVERIFY(c->isOnAllDesktops());
    // double click again
    PRESS;
    RELEASE;
    QVERIFY(c->isOnAllDesktops());
    PRESS;
    RELEASE;
    QVERIFY(!c->isOnAllDesktops());

    // test top most deco pixel, BUG: 362860
    c->move(0, 0);
    QFETCH(QPoint, decoPoint);
    MOTION(decoPoint);
    QVERIFY(!input()->pointer()->decoration().isNull());
    QCOMPARE(input()->pointer()->decoration()->client(), c);
    QTEST(input()->pointer()->decoration()->decoration()->sectionUnderMouse(), "expectedSection");
    // double click
    PRESS;
    RELEASE;
    QVERIFY(!c->isOnAllDesktops());
    PRESS;
    RELEASE;
    QVERIFY(c->isOnAllDesktops());
}

void DecorationInputTest::testDoubleTap_data()
{
    QTest::addColumn<QPoint>("decoPoint");
    QTest::addColumn<Qt::WindowFrameSection>("expectedSection");
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("topLeft") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("top") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("topRight") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::WlShell;
    QTest::newRow("topLeft|xdgv5") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("top|xdgv5") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("topRight|xdgv5") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("topLeft|xdgv6") << QPoint(0, 0) << Qt::TopLeftSection << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("top|xdgv6") << QPoint(250, 0) << Qt::TopSection << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("topRight|xdgv6") << QPoint(499, 0) << Qt::TopRightSection << Test::ShellSurfaceType::XdgShellV6;

}

void KWin::DecorationInputTest::testDoubleTap()
{
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    QVERIFY(!c->isOnAllDesktops());
    quint32 timestamp = 1;
    const QPoint tapPoint(c->geometry().center().x(), c->clientPos().y() / 2);

    // double tap
    kwinApp()->platform()->touchDown(0, tapPoint, timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    kwinApp()->platform()->touchDown(0, tapPoint, timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(c->isOnAllDesktops());
    // double tap again
    kwinApp()->platform()->touchDown(0, tapPoint, timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(c->isOnAllDesktops());
    kwinApp()->platform()->touchDown(0, tapPoint, timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(!c->isOnAllDesktops());

    // test top most deco pixel, BUG: 362860
    c->move(0, 0);
    QFETCH(QPoint, decoPoint);
    // double click
    kwinApp()->platform()->touchDown(0, decoPoint, timestamp++);
    QVERIFY(!input()->touch()->decoration().isNull());
    QCOMPARE(input()->touch()->decoration()->client(), c);
    QTEST(input()->touch()->decoration()->decoration()->sectionUnderMouse(), "expectedSection");
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(!c->isOnAllDesktops());
    kwinApp()->platform()->touchDown(0, decoPoint, timestamp++);
    kwinApp()->platform()->touchUp(0, timestamp++);
    QVERIFY(c->isOnAllDesktops());
}

void DecorationInputTest::testHover_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("wlShell") << Test::ShellSurfaceType::WlShell;
    QTest::newRow("xdgShellV5") << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("xdgShellV6") << Test::ShellSurfaceType::XdgShellV6;
}

void DecorationInputTest::testHover()
{
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());

    // our left border is moved out of the visible area, so move the window to a better place
    c->move(QPoint(20, 0));

    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->clientPos().y() / 2));
    QCOMPARE(c->cursor(), Qt::ArrowCursor);

    MOTION(QPoint(20, 0));
    QCOMPARE(c->cursor(), Qt::SizeFDiagCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() / 2, 0));
    QCOMPARE(c->cursor(), Qt::SizeVerCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() - 1, 0));
    QCOMPARE(c->cursor(), Qt::SizeBDiagCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() - 1, c->height() / 2));
    QCOMPARE(c->cursor(), Qt::SizeHorCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() - 1, c->height() - 1));
    QCOMPARE(c->cursor(), Qt::SizeFDiagCursor);
    MOTION(QPoint(c->geometry().x() + c->geometry().width() / 2, c->height() - 1));
    QCOMPARE(c->cursor(), Qt::SizeVerCursor);
    MOTION(QPoint(c->geometry().x(), c->height() - 1));
    QCOMPARE(c->cursor(), Qt::SizeBDiagCursor);
    MOTION(QPoint(c->geometry().x(), c->height() / 2));
    QCOMPARE(c->cursor(), Qt::SizeHorCursor);

    MOTION(c->geometry().center());
    QEXPECT_FAIL("", "Cursor not set back on leave", Continue);
    QCOMPARE(c->cursor(), Qt::ArrowCursor);
}

void DecorationInputTest::testPressToMove_data()
{
    QTest::addColumn<QPoint>("offset");
    QTest::addColumn<QPoint>("offset2");
    QTest::addColumn<QPoint>("offset3");
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("To right")  << QPoint(10, 0)  << QPoint(20, 0)  << QPoint(30, 0) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To left")   << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To bottom") << QPoint(0, 10)  << QPoint(0, 20)  << QPoint(0, 30) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To top")    << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To right|xdgv5")  << QPoint(10, 0)  << QPoint(20, 0)  << QPoint(30, 0) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To left|xdgv5")   << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To bottom|xdgv5") << QPoint(0, 10)  << QPoint(0, 20)  << QPoint(0, 30) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To top|xdgv5")    << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To right|xdgv6")  << QPoint(10, 0)  << QPoint(20, 0)  << QPoint(30, 0) << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("To left|xdgv6")   << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0) << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("To bottom|xdgv6") << QPoint(0, 10)  << QPoint(0, 20)  << QPoint(0, 30) << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("To top|xdgv6")    << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30) << Test::ShellSurfaceType::XdgShellV6;
}

void DecorationInputTest::testPressToMove()
{
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    c->move(screens()->geometry(0).center() - QPoint(c->width()/2, c->height()/2));
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(c, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    quint32 timestamp = 1;
    MOTION(QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2));
    QCOMPARE(c->cursor(), Qt::ArrowCursor);

    PRESS;
    QVERIFY(!c->isMove());
    QFETCH(QPoint, offset);
    MOTION(QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2) + offset);
    const QPoint oldPos = c->pos();
    QVERIFY(c->isMove());
    QCOMPARE(startMoveResizedSpy.count(), 1);

    RELEASE;
    QTRY_VERIFY(!c->isMove());
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QEXPECT_FAIL("", "Just trigger move doesn't move the window", Continue);
    QCOMPARE(c->pos(), oldPos + offset);

    // again
    PRESS;
    QVERIFY(!c->isMove());
    QFETCH(QPoint, offset2);
    MOTION(QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2) + offset2);
    QVERIFY(c->isMove());
    QCOMPARE(startMoveResizedSpy.count(), 2);
    QFETCH(QPoint, offset3);
    MOTION(QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2) + offset3);

    RELEASE;
    QTRY_VERIFY(!c->isMove());
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 2);
    // TODO: the offset should also be included
    QCOMPARE(c->pos(), oldPos + offset2 + offset3);
}

void DecorationInputTest::testTapToMove_data()
{
    QTest::addColumn<QPoint>("offset");
    QTest::addColumn<QPoint>("offset2");
    QTest::addColumn<QPoint>("offset3");
    QTest::addColumn<Test::ShellSurfaceType>("type");

    QTest::newRow("To right")  << QPoint(10, 0)  << QPoint(20, 0)  << QPoint(30, 0) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To left")   << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To bottom") << QPoint(0, 10)  << QPoint(0, 20)  << QPoint(0, 30) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To top")    << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30) << Test::ShellSurfaceType::WlShell;
    QTest::newRow("To right|xdgv5")  << QPoint(10, 0)  << QPoint(20, 0)  << QPoint(30, 0) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To left|xdgv5")   << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To bottom|xdgv5") << QPoint(0, 10)  << QPoint(0, 20)  << QPoint(0, 30) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To top|xdgv5")    << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30) << Test::ShellSurfaceType::XdgShellV5;
    QTest::newRow("To right|xdgv6")  << QPoint(10, 0)  << QPoint(20, 0)  << QPoint(30, 0) << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("To left|xdgv6")   << QPoint(-10, 0) << QPoint(-20, 0) << QPoint(-30, 0) << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("To bottom|xdgv6") << QPoint(0, 10)  << QPoint(0, 20)  << QPoint(0, 30) << Test::ShellSurfaceType::XdgShellV6;
    QTest::newRow("To top|xdgv6")    << QPoint(0, -10) << QPoint(0, -20) << QPoint(0, -30) << Test::ShellSurfaceType::XdgShellV6;
}

void DecorationInputTest::testTapToMove()
{
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    c->move(screens()->geometry(0).center() - QPoint(c->width()/2, c->height()/2));
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());
    QSignalSpy clientFinishUserMovedResizedSpy(c, &AbstractClient::clientFinishUserMovedResized);
    QVERIFY(clientFinishUserMovedResizedSpy.isValid());

    quint32 timestamp = 1;
    QPoint p = QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2);

    kwinApp()->platform()->touchDown(0, p, timestamp++);
    QVERIFY(!c->isMove());
    QFETCH(QPoint, offset);
    QCOMPARE(input()->touch()->decorationPressId(), 0);
    kwinApp()->platform()->touchMotion(0, p + offset, timestamp++);
    const QPoint oldPos = c->pos();
    QVERIFY(c->isMove());
    QCOMPARE(startMoveResizedSpy.count(), 1);

    kwinApp()->platform()->touchUp(0, timestamp++);
    QTRY_VERIFY(!c->isMove());
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 1);
    QEXPECT_FAIL("", "Just trigger move doesn't move the window", Continue);
    QCOMPARE(c->pos(), oldPos + offset);

    // again
    kwinApp()->platform()->touchDown(1, p + offset, timestamp++);
    QCOMPARE(input()->touch()->decorationPressId(), 1);
    QVERIFY(!c->isMove());
    QFETCH(QPoint, offset2);
    kwinApp()->platform()->touchMotion(1, QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2) + offset2, timestamp++);
    QVERIFY(c->isMove());
    QCOMPARE(startMoveResizedSpy.count(), 2);
    QFETCH(QPoint, offset3);
    kwinApp()->platform()->touchMotion(1, QPoint(c->geometry().center().x(), c->y() + c->clientPos().y() / 2) + offset3, timestamp++);

    kwinApp()->platform()->touchUp(1, timestamp++);
    QTRY_VERIFY(!c->isMove());
    QCOMPARE(clientFinishUserMovedResizedSpy.count(), 2);
    // TODO: the offset should also be included
    QCOMPARE(c->pos(), oldPos + offset2 + offset3);
}

void DecorationInputTest::testResizeOutsideWindow_data()
{
    QTest::addColumn<Test::ShellSurfaceType>("type");
    QTest::addColumn<Qt::Edge>("edge");
    QTest::addColumn<Qt::CursorShape>("expectedCursor");

    QTest::newRow("wlShell - left") << Test::ShellSurfaceType::WlShell << Qt::LeftEdge << Qt::SizeHorCursor;
    QTest::newRow("xdgShellV5 - left") << Test::ShellSurfaceType::XdgShellV5 << Qt::LeftEdge << Qt::SizeHorCursor;
    QTest::newRow("xdgShellV6 - left") << Test::ShellSurfaceType::XdgShellV6 << Qt::LeftEdge << Qt::SizeHorCursor;
    QTest::newRow("wlShell - right") << Test::ShellSurfaceType::WlShell << Qt::RightEdge << Qt::SizeHorCursor;
    QTest::newRow("xdgShellV5 - right") << Test::ShellSurfaceType::XdgShellV5 << Qt::RightEdge << Qt::SizeHorCursor;
    QTest::newRow("xdgShellV6 - right") << Test::ShellSurfaceType::XdgShellV6 << Qt::RightEdge << Qt::SizeHorCursor;
    QTest::newRow("wlShell - bottom") << Test::ShellSurfaceType::WlShell << Qt::BottomEdge << Qt::SizeVerCursor;
    QTest::newRow("xdgShellV5 - bottom") << Test::ShellSurfaceType::XdgShellV5 << Qt::BottomEdge << Qt::SizeVerCursor;
    QTest::newRow("xdgShellV6 - bottom") << Test::ShellSurfaceType::XdgShellV6 << Qt::BottomEdge << Qt::SizeVerCursor;
}

void DecorationInputTest::testResizeOutsideWindow()
{
    // this test verifies that one can resize the window outside the decoration with NoSideBorder

    // first adjust config
    kwinApp()->config()->group("org.kde.kdecoration2").writeEntry("BorderSize", QStringLiteral("None"));
    kwinApp()->config()->sync();
    workspace()->slotReconfigure();

    // now create window
    QFETCH(Test::ShellSurfaceType, type);
    AbstractClient *c = showWindow(type);
    QVERIFY(c);
    QVERIFY(c->isDecorated());
    QVERIFY(!c->noBorder());
    c->move(screens()->geometry(0).center() - QPoint(c->width()/2, c->height()/2));
    QVERIFY(c->geometry() != c->inputGeometry());
    QVERIFY(c->inputGeometry().contains(c->geometry()));
    QSignalSpy startMoveResizedSpy(c, &AbstractClient::clientStartUserMovedResized);
    QVERIFY(startMoveResizedSpy.isValid());

    // go to border
    quint32 timestamp = 1;
    QFETCH(Qt::Edge, edge);
    switch (edge) {
    case Qt::LeftEdge:
        MOTION(QPoint(c->geometry().x() -1, c->geometry().center().y()));
        break;
    case Qt::RightEdge:
        MOTION(QPoint(c->geometry().x() + c->geometry().width() +1, c->geometry().center().y()));
        break;
    case Qt::BottomEdge:
        MOTION(QPoint(c->geometry().center().x(), c->geometry().y() + c->geometry().height() + 1));
        break;
    default:
        break;
    }
    QVERIFY(!c->geometry().contains(KWin::Cursor::pos()));

    // pressing should trigger resize
    PRESS;
    QVERIFY(!c->isResize());
    QVERIFY(startMoveResizedSpy.wait());
    QVERIFY(c->isResize());

    RELEASE;
    QVERIFY(!c->isResize());
}

}

WAYLANDTEST_MAIN(KWin::DecorationInputTest)
#include "decoration_input_test.moc"
