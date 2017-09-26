/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2007 Lubos Lunak <l.lunak@kde.org>

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

#include "fallapart.h"
// KConfigSkeleton
#include "fallapartconfig.h"
#include <assert.h>
#include <time.h>
#include <math.h>

namespace KWin
{

bool FallApartEffect::supported()
{
    return effects->isOpenGLCompositing() && effects->animationsSupported();
}

FallApartEffect::FallApartEffect()
{
    initConfig<FallApartConfig>();
    reconfigure(ReconfigureAll);
    connect(effects, SIGNAL(windowClosed(KWin::EffectWindow*)), this, SLOT(slotWindowClosed(KWin::EffectWindow*)));
    connect(effects, SIGNAL(windowDeleted(KWin::EffectWindow*)), this, SLOT(slotWindowDeleted(KWin::EffectWindow*)));
}

void FallApartEffect::reconfigure(ReconfigureFlags)
{
    FallApartConfig::self()->read();
    blockSize = FallApartConfig::blockSize();
    seedStart = time(NULL);
    totalWindowsSeen = 0;
}

void FallApartEffect::prePaintScreen(ScreenPrePaintData& data, int time)
{
    if (!windows.isEmpty())
        data.mask |= PAINT_SCREEN_WITH_TRANSFORMED_WINDOWS;
    effects->prePaintScreen(data, time);
}

void FallApartEffect::prePaintWindow(EffectWindow* w, WindowPrePaintData& data, int time)
{
    if (windows.contains(w) && isRealWindow(w)) {
        if (windows[ w ].time < 1) {
            windows[ w ].time += time / animationTime(1000.);
            data.setTransformed();
            w->enablePainting(EffectWindow::PAINT_DISABLED_BY_DELETE);
            // Request the window to be divided into cells
            data.quads = data.quads.makeGrid(blockSize);
        } else {
            windows.remove(w);
            w->unrefWindow();
        }
    }
    effects->prePaintWindow(w, data, time);
}

void FallApartEffect::paintWindow(EffectWindow* w, int mask, QRegion region, WindowPaintData& data)
{
    if (windows.contains(w) && isRealWindow(w)) {
        // change direction randomly but consistently
        unsigned int windowSeed = windows[ w ].seed;

        WindowQuadList new_quads;
        foreach (WindowQuad quad, data.quads) { // krazy:exclude=foreach
            // make fragments move in various directions, based on where
            // they are (left pieces generally move to the left, etc.)
            QPointF p1(quad[ 0 ].x(), quad[ 0 ].y());
            double xdiff = 0;
            if (p1.x() < w->width() / 2)
                xdiff = -(w->width() / 2 - p1.x()) / w->width() * 10;
            if (p1.x() > w->width() / 2)
                xdiff = (p1.x() - w->width() / 2) / w->width() * 10;
            double ydiff = 0;
            if (p1.y() < w->height() / 2)
                ydiff = -(w->height() / 2 - p1.y()) / w->height() * 10 + pow(1.07, windows[ w ].time * 50);
            if (p1.y() > w->height() / 2)
                ydiff = (p1.y() - w->height() / 2) / w->height() * 10 + pow(1.07, windows[ w ].time * 50);
            double modif = windows[ w ].time * windows[ w ].time * 64;
            xdiff += rand_r(&windowSeed) % 11 - 5;
            ydiff += rand_r(&windowSeed) % 21 - 5;
            for (int j = 0;
                    j < 4;
                    ++j) {
                quad[ j ].move(quad[ j ].x() + xdiff * modif, quad[ j ].y() + ydiff * modif);
            }
            // also make the fragments rotate around their center
            QPointF center((quad[ 0 ].x() + quad[ 1 ].x() + quad[ 2 ].x() + quad[ 3 ].x()) / 4,
                           (quad[ 0 ].y() + quad[ 1 ].y() + quad[ 2 ].y() + quad[ 3 ].y()) / 4);
            double adiff = (rand_r(&windowSeed) % 720 - 360) / 360. * 2 * M_PI;   // spin randomly
            for (int j = 0;
                    j < 4;
                    ++j) {
                double x = quad[ j ].x() - center.x();
                double y = quad[ j ].y() - center.y();
                double angle = atan2(y, x);
                angle += windows[ w ].time * adiff;
                double dist = sqrt(x * x + y * y);
                x = dist * cos(angle);
                y = dist * sin(angle);
                quad[ j ].move(center.x() + x, center.y() + y);
            }
            new_quads.append(quad);
        }
        data.quads = new_quads;
    }
    effects->paintWindow(w, mask, region, data);
}

void FallApartEffect::postPaintScreen()
{
    if (!windows.isEmpty())
        effects->addRepaintFull();
    effects->postPaintScreen();
}

bool FallApartEffect::isRealWindow(EffectWindow* w)
{
    // TODO: isSpecialWindow is rather generic, maybe tell windowtypes separately?
    /*
    qCDebug(KWINEFFECTS) << "--" << w->caption() << "--------------------------------";
    qCDebug(KWINEFFECTS) << "Tooltip:" << w->isTooltip();
    qCDebug(KWINEFFECTS) << "Toolbar:" << w->isToolbar();
    qCDebug(KWINEFFECTS) << "Desktop:" << w->isDesktop();
    qCDebug(KWINEFFECTS) << "Special:" << w->isSpecialWindow();
    qCDebug(KWINEFFECTS) << "TopMenu:" << w->isTopMenu();
    qCDebug(KWINEFFECTS) << "Notific:" << w->isNotification();
    qCDebug(KWINEFFECTS) << "Splash:" << w->isSplash();
    qCDebug(KWINEFFECTS) << "Normal:" << w->isNormalWindow();
    */
    return w->isNormalWindow();
}

void FallApartEffect::slotWindowClosed(EffectWindow* c)
{
    if (!isRealWindow(c))
        return;
    if (!c->isVisible())
        return;
    const void* e = c->data(WindowClosedGrabRole).value<void*>();
    if (e && e != this)
        return;
    windows[ c ].time = 0;
    windows[ c ].seed = seedStart + totalWindowsSeen++;
    c->refWindow();
}

void FallApartEffect::slotWindowDeleted(EffectWindow* c)
{
    windows.remove(c);
}

bool FallApartEffect::isActive() const
{
    return !windows.isEmpty();
}

} // namespace
