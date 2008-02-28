/*
 QC_QRegion.h
 
 Qore Programming Language
 
 Copyright (C) 2003, 2004, 2005, 2006, 2007 David Nichols
 
 This library is free software; you can redistribute it and/or
 modify it under the terms of the GNU Lesser General Public
 License as published by the Free Software Foundation; either
 version 2.1 of the License, or (at your option) any later version.
 
 This library is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 Lesser General Public License for more details.
 
 You should have received a copy of the GNU Lesser General Public
 License along with this library; if not, write to the Free Software
 Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
 */

#ifndef _QORE_QC_QREGION_H

#define _QORE_QC_QREGION_H

#include <QRegion>

DLLLOCAL extern qore_classid_t CID_QREGION;
DLLLOCAL extern class QoreClass *QC_QRegion;

DLLLOCAL class QoreClass *initQRegionClass();

class QoreQRegion : public AbstractPrivateData, public QRegion
{
   public:
      DLLLOCAL QoreQRegion() : QRegion()
      {
      }
      DLLLOCAL QoreQRegion(int x, int y, int width, int height, QRegion::RegionType t = QRegion::Rectangle) : QRegion(x, y, width, height, t)
      {
      }
      DLLLOCAL QoreQRegion(const QRegion &qr) : QRegion(qr)
      {
      }
      DLLLOCAL QoreQRegion(const QRect &qr, QRegion::RegionType t = QRegion::Rectangle) : QRegion(qr, t)
      {
      }
      DLLLOCAL QoreQRegion(const QPolygon &pa, Qt::FillRule fillRule = Qt::OddEvenFill) : QRegion(pa, fillRule)
      {
      }
};


#endif
