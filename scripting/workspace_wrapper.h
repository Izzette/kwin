/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2010 Rohan Prabhu <rohan@rohanprabhu.com>
Copyright (C) 2012 Martin Gräßlin <mgraesslin@kde.org>

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

#ifndef KWIN_SCRIPTING_WORKSPACE_WRAPPER_H
#define KWIN_SCRIPTING_WORKSPACE_WRAPPER_H

#include <QtCore/QObject>
#include <QtCore/QSize>

namespace KWin
{
// forward declarations
class Client;

class WorkspaceWrapper : public QObject
{
    Q_OBJECT
    Q_PROPERTY(int currentDesktop READ currentDesktop WRITE setCurrentDesktop NOTIFY currentDesktopChanged)
    Q_PROPERTY(KWin::Client *activeClient READ activeClient WRITE setActiveClient NOTIFY clientActivated)
    // TODO: write and notify?
    Q_PROPERTY(QSize desktopGridSize READ desktopGridSize)
    Q_PROPERTY(int desktopGridWidth READ desktopGridWidth)
    Q_PROPERTY(int desktopGridHeight READ desktopGridHeight)
    Q_PROPERTY(int workspaceWidth READ workspaceWidth)
    Q_PROPERTY(int workspaceHeight READ workspaceHeight)
    Q_PROPERTY(QSize workspaceSize READ workspaceSize)
    /**
     * The number of desktops currently used. Minimum number of desktops is 1, maximum 20.
     **/
    Q_PROPERTY(int desktops READ numberOfDesktops WRITE setNumberOfDesktops NOTIFY numberDesktopsChanged)
    /**
     * The same of the display, that is all screens.
     **/
    Q_PROPERTY(QSize displaySize READ displaySize)
    /**
     * The width of the display, that is width of all combined screens.
     **/
    Q_PROPERTY(int displayWidth READ displayWidth)
    /**
     * The height of the display, that is height of all combined screens.
     **/
    Q_PROPERTY(int displayHeight READ displayHeight)

private:
    Q_DISABLE_COPY(WorkspaceWrapper)

signals:
    void desktopPresenceChanged(KWin::Client*, int);
    void currentDesktopChanged(int);
    void clientAdded(KWin::Client*);
    void clientRemoved(KWin::Client*);
    void clientManaging(KWin::Client*);
    void clientMinimized(KWin::Client*);
    void clientUnminimized(KWin::Client*);
    void clientRestored(KWin::Client*);
    void clientMaximizeSet(KWin::Client*, bool, bool);
    void killWindowCalled(KWin::Client*);
    void clientActivated(KWin::Client*);
    void clientFullScreenSet(KWin::Client*, bool, bool);
    void clientSetKeepAbove(KWin::Client*, bool);
    /**
     * Signal emitted whenever the number of desktops changed.
     * To get the current number of desktops use the property desktops.
     * @param oldNumberOfDesktops The previous number of desktops.
     **/
    void numberDesktopsChanged(int oldNumberOfDesktops);

public:
    WorkspaceWrapper(QObject* parent = 0);
#define GETTERSETTERDEF( rettype, getter, setter ) \
rettype getter() const; \
void setter( rettype val );
    GETTERSETTERDEF(int, numberOfDesktops, setNumberOfDesktops)
    GETTERSETTERDEF(int, currentDesktop, setCurrentDesktop)
    GETTERSETTERDEF(KWin::Client*, activeClient, setActiveClient)
#undef GETTERSETTERDEF
    QSize desktopGridSize() const;
    int desktopGridWidth() const;
    int desktopGridHeight() const;
    int workspaceWidth() const;
    int workspaceHeight() const;
    QSize workspaceSize() const;
    int displayWidth() const;
    int displayHeight() const;
    QSize displaySize() const;

    Q_INVOKABLE QList< KWin::Client* > clientList() const;

private Q_SLOTS:
    void setupClientConnections(KWin::Client* client);
};

}

#endif
