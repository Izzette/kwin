/*
 *
 * Copyright (c) 2003 Lubos Lunak <l.lunak@kde.org>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include "preview.h"

#include <kapplication.h>
#include <klocale.h>
#include <kconfig.h>
#include <kglobal.h>
#include <qlabel.h>
#include <qstyle.h>
#include <kiconloader.h>

#include <X11/Xlib.h>
#include <X11/extensions/shape.h>

#include <kdecoration_plugins_p.h>

// FRAME the preview doesn't update to reflect the changes done in the kcm

KDecorationPreview::KDecorationPreview( QWidget* parent, const char* name )
    :   QWidget( parent, name )
    {
    options = new KDecorationPreviewOptions;
    
    bridge[Active]   = new KDecorationPreviewBridge( this, true );
    bridge[Inactive] = new KDecorationPreviewBridge( this, false );

    deco[Active] = deco[Inactive] = NULL;

    no_preview = new QLabel( i18n( "No preview available.\n"
                                   "Most probably there\n"
                                   "was a problem loading the plugin." ), this );

    no_preview->setAlignment( AlignCenter );

    setMinimumSize( 100, 100 );
    no_preview->resize( size());
    }

KDecorationPreview::~KDecorationPreview()
    {
    for ( int i = 0; i < NumWindows; i++ )
        {
        delete deco[i];
        delete bridge[i];
	}
    delete options;
    }

bool KDecorationPreview::recreateDecoration( KDecorationPlugins* plugins )
    {
    for ( int i = 0; i < NumWindows; i++ )
        {
        delete deco[i];   // deletes also window
        deco[i] = plugins->createDecoration( bridge[i] );
        deco[i]->init();
        }

    if( deco[Active] == NULL || deco[Inactive] == NULL )
        {
        return false;
        }
  
    positionPreviews();
    deco[Inactive]->widget()->show();
    deco[Active]->widget()->show();

    return true;
    }

void KDecorationPreview::enablePreview()
    {
    no_preview->hide();
    }
    
void KDecorationPreview::disablePreview()
    {
    delete deco[Active];
    delete deco[Inactive];
    deco[Active] = deco[Inactive] = NULL;
    no_preview->show();
    }

void KDecorationPreview::resizeEvent( QResizeEvent* e )
    {
    QWidget::resizeEvent( e );
    positionPreviews();
    }
    
void KDecorationPreview::positionPreviews()
    {
    int titleBarHeight, leftBorder, rightBorder, xoffset, dummy;
    QRect geometry;
    QSize size;
 
    no_preview->resize( this->size() );

    if ( !deco[Active] || !deco[Inactive] )
        return; 

    deco[Active]->borders( dummy, dummy, titleBarHeight, dummy );
    deco[Inactive]->borders( leftBorder, rightBorder, dummy, dummy );

    titleBarHeight = kMin( int( titleBarHeight * .9 ), 30 );
    xoffset = kMin( kMax( 10, QApplication::reverseLayout()
			    ? leftBorder : rightBorder ), 30 );

    // Resize the active window
    size = QSize( width() - xoffset, height() - titleBarHeight )
                .expandedTo( deco[Active]->minimumSize() );
    geometry = QRect( QPoint( 0, titleBarHeight ), size );
    deco[Active]->widget()->setGeometry( QStyle::visualRect( geometry, this ) );

    // Resize the inactive window
    size = QSize( width() - xoffset, height() - titleBarHeight )
                .expandedTo( deco[Inactive]->minimumSize() );
    geometry = QRect( QPoint( xoffset, 0 ), size );
    deco[Inactive]->widget()->setGeometry( QStyle::visualRect( geometry, this ) );
    }

void KDecorationPreview::setPreviewMask( const QRegion& reg, int mode, bool active )
    {
    QWidget *widget = active ? deco[Active]->widget() : deco[Inactive]->widget();

    // FRAME duped from client.cpp
    if( mode == Unsorted )
        {
        XShapeCombineRegion( qt_xdisplay(), widget->winId(), ShapeBounding, 0, 0,
            reg.handle(), ShapeSet );
        }
    else
        {
        QMemArray< QRect > rects = reg.rects();
        XRectangle* xrects = new XRectangle[ rects.count() ];
        for( unsigned int i = 0;
             i < rects.count();
             ++i )
            {
            xrects[ i ].x = rects[ i ].x();
            xrects[ i ].y = rects[ i ].y();
            xrects[ i ].width = rects[ i ].width();
            xrects[ i ].height = rects[ i ].height();
            }
        XShapeCombineRectangles( qt_xdisplay(), widget->winId(), ShapeBounding, 0, 0,
	    xrects, rects.count(), ShapeSet, mode );
        delete[] xrects;
        }
    if( active )
        mask = reg; // keep shape of the active window for unobscuredRegion()
    }

QRect KDecorationPreview::windowGeometry( bool active ) const
    {
    QWidget *widget = active ? deco[Active]->widget() : deco[Inactive]->widget();
    return widget->geometry();
    }

QRegion KDecorationPreview::unobscuredRegion( bool active, const QRegion& r ) const
    {
    if( active ) // this one is not obscured
        return r;
    else
        {
        // copied from KWin core's code
        QRegion ret = r;
        QRegion r2 = mask;
        if( r2.isEmpty())
            r2 = QRegion( windowGeometry( true ));
        r2.translate( windowGeometry( true ).x() - windowGeometry( false ).x(),
            windowGeometry( true ).y() - windowGeometry( false ).y());
        ret -= r2;
        return ret;
        }
    }

KDecorationPreviewBridge::KDecorationPreviewBridge( KDecorationPreview* p, bool a )
    :   preview( p ), active( a )
    {
    }

QWidget* KDecorationPreviewBridge::initialParentWidget() const
    {
    return preview;
    }
    
Qt::WFlags KDecorationPreviewBridge::initialWFlags() const
    {
    return 0;
    }

bool KDecorationPreviewBridge::isActive() const
    {
    return active;
    }
    
bool KDecorationPreviewBridge::isCloseable() const
    {
    return true;
    }
    
bool KDecorationPreviewBridge::isMaximizable() const
    {
    return true;
    }
    
KDecoration::MaximizeMode KDecorationPreviewBridge::maximizeMode() const
    {
    return KDecoration::MaximizeRestore;
    }
    
bool KDecorationPreviewBridge::isMinimizable() const
    {
    return true;
    }
    
bool KDecorationPreviewBridge::providesContextHelp() const
    {
    return true;
    }
    
int KDecorationPreviewBridge::desktop() const
    {
    return 1;
    }
    
bool KDecorationPreviewBridge::isModal() const
    {
    return false;
    }
    
bool KDecorationPreviewBridge::isShadeable() const
    {
    return true;
    }
    
bool KDecorationPreviewBridge::isShade() const
    {
    return false;
    }
    
bool KDecorationPreviewBridge::isSetShade() const
    {
    return false;
    }
    
bool KDecorationPreviewBridge::keepAbove() const
    {
    return false;
    }
    
bool KDecorationPreviewBridge::keepBelow() const
    {
    return false;
    }
    
bool KDecorationPreviewBridge::isMovable() const
    {
    return true;
    }
    
bool KDecorationPreviewBridge::isResizable() const
    {
    return true;
    }
    
NET::WindowType KDecorationPreviewBridge::windowType( unsigned long ) const
    {
    return NET::Normal;
    }
    
QIconSet KDecorationPreviewBridge::icon() const
    {
    return SmallIconSet( "xapp" );
    }

QString KDecorationPreviewBridge::caption() const
    {
    return active ? i18n( "Active window" ) : i18n( "Inactive window" );
    }
    
void KDecorationPreviewBridge::processMousePressEvent( QMouseEvent* )
    {
    }
    
void KDecorationPreviewBridge::showWindowMenu( QPoint )
    {
    }
    
void KDecorationPreviewBridge::performWindowOperation( WindowOperation )
    {
    }
    
void KDecorationPreviewBridge::setMask( const QRegion& reg, int mode )
    {
    preview->setPreviewMask( reg, mode, active );
    }
    
bool KDecorationPreviewBridge::isPreview() const
    {
    return true;
    }

QRect KDecorationPreviewBridge::geometry() const
    {
    return preview->windowGeometry( active );
    }
        
QRect KDecorationPreviewBridge::iconGeometry() const
    {
    return QRect();
    }

QRegion KDecorationPreviewBridge::unobscuredRegion( const QRegion& r ) const
    {
    return preview->unobscuredRegion( active, r );
    }

QWidget* KDecorationPreviewBridge::workspaceWidget() const
    {
    return preview;
    }
        
void KDecorationPreviewBridge::closeWindow()
    {
    }
    
void KDecorationPreviewBridge::maximize( MaximizeMode )
    {
    }
    
void KDecorationPreviewBridge::minimize()
    {
    }
    
void KDecorationPreviewBridge::showContextHelp()
    {
    }
    
void KDecorationPreviewBridge::setDesktop( int )
    {
    }
    
void KDecorationPreviewBridge::titlebarDblClickOperation()
    {
    }
    
void KDecorationPreviewBridge::setShade( bool )
    {
    }
    
void KDecorationPreviewBridge::setKeepAbove( bool )
    {
    }
    
void KDecorationPreviewBridge::setKeepBelow( bool )
    {
    }
    
int KDecorationPreviewBridge::currentDesktop() const
    {
    return 1;
    }

void KDecorationPreviewBridge::helperShowHide( bool )
    {
    }

void KDecorationPreviewBridge::grabXServer( bool )
    {
    }

KDecorationPreviewOptions::KDecorationPreviewOptions()
    {
    d = new KDecorationOptionsPrivate;
    d->defaultKWinSettings();
    updateSettings();
    }

KDecorationPreviewOptions::~KDecorationPreviewOptions()
    {
    delete d;
    }

unsigned long KDecorationPreviewOptions::updateSettings()
    {
    KConfig cfg( "kwinrc", true );
    unsigned long changed = 0;
    changed |= d->updateKWinSettings( &cfg );
    return changed;
    }

bool KDecorationPreviewPlugins::provides( Requirement )
    {
    return false;
    }
    
#include "preview.moc"
