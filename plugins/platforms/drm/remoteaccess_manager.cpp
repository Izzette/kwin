/********************************************************************
 KWin - the KDE window manager
 This file is part of the KDE project.

Copyright (C) 2016 Oleg Chernovskiy <kanedias@xaker.ru>

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
#include "remoteaccess_manager.h"
#include "logging.h"

// system
#include <unistd.h>
#include <gbm.h>

namespace KWin
{

RemoteAccessManager::RemoteAccessManager(QObject *parent)
    : QObject(parent)
{
    if (waylandServer()) {
        m_interface = waylandServer()->display()->createRemoteAccessManager(this);
        m_interface->create();

        connect(m_interface, &RemoteAccessManagerInterface::bufferReleased,
                this, &RemoteAccessManager::releaseBuffer);
    }
}

RemoteAccessManager::~RemoteAccessManager()
{
    if (m_interface) {
        m_interface->destroy();
    }
}

void RemoteAccessManager::releaseBuffer(const BufferHandle *buf)
{
    int ret = close(buf->fd());
    if (Q_UNLIKELY(ret)) {
        qCWarning(KWIN_DRM) << "Couldn't close released GBM fd:" << strerror(errno);
    }
    delete buf;
}

void RemoteAccessManager::passBuffer(DrmBuffer *buffer)
{
    DrmSurfaceBuffer* gbmbuf = static_cast<DrmSurfaceBuffer *>(buffer);

    // no connected RemoteAccess instance
    if (!m_interface || !m_interface->isBound()) {
        return;
    }

    // first buffer may be null
    if (!gbmbuf || !gbmbuf->hasBo()) {
        return;
    }

    qCDebug(KWIN_DRM) << "Handing over GBM object to remote framebuffer";
    auto buf = new BufferHandle;
    buf->setFd(gbm_bo_get_fd(gbmbuf->getBo()));
    buf->setSize(gbm_bo_get_width(gbmbuf->getBo()), gbm_bo_get_height(gbmbuf->getBo()));
    buf->setStride(gbm_bo_get_stride(gbmbuf->getBo()));
    buf->setFormat(gbm_bo_get_format(gbmbuf->getBo()));

    qCDebug(KWIN_DRM) << "Buffer passed: bo" << gbmbuf->getBo() << ", fd" << buf->fd();

    m_interface->sendBufferReady(buf);
}

} // KWin namespace
