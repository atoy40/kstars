/*
    SPDX-FileCopyrightText: 2021 Hy Murveit <hy@murveit.com>

    SPDX-License-Identifier: GPL-2.0-or-later
*/

#include "polaralign.h"
#include "poleaxis.h"
#include "rotations.h"

#include <cmath>

#include "fitsviewer/fitsdata.h"
#include "kstarsdata.h"
#include "skypoint.h"
#include <ekos_align_debug.h>

/******************************************************************
PolarAlign is a class that supports polar alignment by determining the
mount's axis of rotation when given 3 solved images taken with RA mount
rotations between the images.

addPoint(image) is called by the polar alignment UI after it takes and
solves each of its three images. The solutions are store in SkyPoints (see below)
and are processed so that the sky positions correspond to "what's in the sky
now" and "at this geographic localtion".

Addpoint() samples the location of a particular pixel in its image.
When the 3 points are sampled, they should not be taken
from the center of the image, as HA rotations may not move that point
if the telescope and mount are well aligned. Thus, the points are sampled
from the edge of the image.

After all 3 images are sampled, findAxis() is called, which solves for the mount's
axis of rotation. It then transforms poleAxis' result into azimuth and altitude
offsets from the pole.

findCorrectedPixel() is given an x,y position on an image, and the offsets
generated by findAxis(), and computes a "corrected position" for that input
x,y point, such that if a user adjusted the GEM mount's altitude and azimuth
controls to move an object in the image's original x,y position to the corrected
position in the image,  the mount's axis of rotation should then coincide with the pole.
******************************************************************/

using Rotations::V3;

PolarAlign::PolarAlign(const GeoLocation *geo)
{
    if (geo == nullptr && KStarsData::Instance() != nullptr)
        geoLocation = KStarsData::Instance()->geo();
    else
        geoLocation = geo;
}

bool PolarAlign::northernHemisphere() const
{
    if ((geoLocation == nullptr) || (geoLocation->lat() == nullptr))
        return true;
    return geoLocation->lat()->Degrees() > 0;
}

void PolarAlign::reset()
{
    points.clear();
    times.clear();
}

// Gets the pixel's j2000 RA&DEC coordinates, converts to JNow,  adjust to
// the local time, and sets up the azimuth and altitude coordinates.
bool PolarAlign::prepareAzAlt(const QSharedPointer<FITSData> &image, const QPointF &pixel, SkyPoint *point) const
{
    // WCS must be set up for this image.
    SkyPoint coords;
    if (image->pixelToWCS(pixel, coords))
    {
        coords.apparentCoord(static_cast<long double>(J2000), image->getDateTime().djd());
        *point = SkyPoint::timeTransformed(&coords, image->getDateTime(), geoLocation, 0);
        return true;
    }
    return false;
}

bool PolarAlign::addPoint(const QSharedPointer<FITSData> &image)
{
    SkyPoint coords;
    auto time = image->getDateTime();
    // Use the HA and DEC from the center of the image.
    if (!prepareAzAlt(image, QPointF(image->width() / 2, image->height() / 2), &coords))
        return false;

    QString debugString = QString("PAA: addPoint ra0 %1 dec0 %2 ra %3 dec %4 az %5 alt %6")
                          .arg(coords.ra0().Degrees()).arg(coords.dec0().Degrees())
                          .arg(coords.ra().Degrees()).arg(coords.dec().Degrees())
                          .arg(coords.az().Degrees()).arg(coords.alt().Degrees());
    qCDebug(KSTARS_EKOS_ALIGN) << debugString;
    if (points.size() > 2)
        return false;
    points.push_back(coords);
    times.push_back(time);

    return true;
}

bool PolarAlign::findAxis()
{
    if (points.size() != 3)
        return false;

    // We have 3 points, get their xyz positions.
    V3 p1(Rotations::azAlt2xyz(QPointF(points[0].az().Degrees(), points[0].alt().Degrees())));
    V3 p2(Rotations::azAlt2xyz(QPointF(points[1].az().Degrees(), points[1].alt().Degrees())));
    V3 p3(Rotations::azAlt2xyz(QPointF(points[2].az().Degrees(), points[2].alt().Degrees())));
    V3 axis = Rotations::getAxis(p1, p2, p3);

    if (axis.length() < 0.9)
    {
        // It failed to normalize the vector, something's wrong.
        qCDebug(KSTARS_EKOS_ALIGN) << "Normal vector too short. findAxis failed.";
        return false;
    }

    // Need to make sure we're pointing to the right pole.
    if ((northernHemisphere() && (axis.x() < 0)) || (!northernHemisphere() && axis.x() > 0))
    {
        axis = V3(-axis.x(), -axis.y(), -axis.z());
    }

    QPointF azAlt = Rotations::xyz2azAlt(axis);
    azimuthCenter = azAlt.x();
    altitudeCenter = azAlt.y();

    return true;
}

void PolarAlign::getAxis(double *azAxis, double *altAxis) const
{
    *azAxis = azimuthCenter;
    *altAxis = altitudeCenter;
}

// Find the pixel in image corresponding to the desired azimuth & altitude.
bool PolarAlign::findAzAlt(const QSharedPointer<FITSData> &image, double azimuth, double altitude, QPointF *pixel) const
{
    SkyPoint spt;
    spt.setAz(azimuth);
    spt.setAlt(altitude);
    dms LST = geoLocation->GSTtoLST(image->getDateTime().gst());
    spt.HorizontalToEquatorial(&LST, geoLocation->lat());
    SkyPoint j2000Coord = spt.catalogueCoord(image->getDateTime().djd());
    QPointF imagePoint;
    if (!image->wcsToPixel(j2000Coord, *pixel, imagePoint))
    {
        QString debugString =
            QString("PolarAlign: Couldn't get pixel from WCS for az %1 alt %2 with j2000 RA %3 DEC %4")
            .arg(QString::number(azimuth), QString::number(altitude), j2000Coord.ra0().toHMSString(), j2000Coord.dec0().toDMSString());
        qCDebug(KSTARS_EKOS_ALIGN) << debugString;
        return false;
    }
    return true;
}

// Calculate the mount's azimuth and altitude error given the known geographic location
// and the azimuth center and altitude center computed in findAxis().
void PolarAlign::calculateAzAltError(double *azError, double *altError) const
{

    const double latitudeDegrees = geoLocation->lat()->Degrees();
    *altError = northernHemisphere() ?
                altitudeCenter - latitudeDegrees : altitudeCenter + latitudeDegrees;
    *azError = northernHemisphere() ? azimuthCenter : azimuthCenter + 180.0;
    while (*azError > 180.0)
        *azError -= 360;
}

// Given the currently estimated RA axis polar alignment error, and given a start pixel,
// find the polar-alignment error if the user moves a star (from his point of view)
// from that pixel to pixel2.
//
// FindCorrectedPixel() determines where the user should move the star to fully correct
// the alignment error. However, while the user is doing that, he/she may be at an intermediate
// point (pixel2) and we want to feed back to the user what the "current" polar-alignment error is.
// This searches using findCorrectedPixel() to
// find the RA axis error which would be fixed by the user moving pixel to pixel2. The input
// thus should be pixel = "current star position", and pixel2 = "solution star position"
// from the original call to findCorrectedPixel. This calls findCorrectedPixel several hundred times
// but is not too costly (about .1s on a RPi4).  One could write a method that more directly estimates
// the error given the current position, but it might not be applicable to our use-case as
// we are constrained to move along paths detemined by a user adjusting an altitude knob and then
// an azimuth adjustment. These corrections are likely not the most direct path to solve the axis error.
bool PolarAlign::pixelError(const QSharedPointer<FITSData> &image, const QPointF &pixel, const QPointF &pixel2,
                            double *azError, double *altError)
{
    double azOffset, altOffset;
    calculateAzAltError(&azOffset, &altOffset);

    QPointF pix;
    double azE, altE;

    pixelError(image, pixel, pixel2, -2.0, 2.0, 0.2, -2.0, 2.0, 0.2, &azE, &altE, &pix);
    pixelError(image, pixel, pixel2, azE - .2, azE + .2, 0.02,
               altE - .2, altE + .2, 0.02, &azE, &altE, &pix);
    pixelError(image, pixel, pixel2, azE - .02, azE + .02, 0.002,
               altE - .02, altE + .02, 0.002, &azE, &altE, &pix);

    const double pixDist = hypot(pix.x() - pixel2.x(), pix.y() - pixel2.y());
    if (pixDist > 10)
        return false;

    *azError = azE;
    *altError = altE;
    return true;
}

void PolarAlign::pixelError(const QSharedPointer<FITSData> &image, const QPointF &pixel, const QPointF &pixel2,
                            double minAz, double maxAz, double azInc,
                            double minAlt, double maxAlt, double altInc,
                            double *azError, double *altError, QPointF *actualPixel)
{
    double minDistSq = 1e9;
    for (double eAz = minAz; eAz < maxAz; eAz += azInc)
    {
        for (double eAlt = minAlt; eAlt < maxAlt; eAlt += altInc)
        {
            QPointF pix;
            if (findCorrectedPixel(image, pixel, &pix, eAz, eAlt))
            {
                // compare the distance to the pixel
                double distSq = ((pix.x() - pixel2.x()) * (pix.x() - pixel2.x()) +
                                 (pix.y() - pixel2.y()) * (pix.y() - pixel2.y()));
                if (distSq < minDistSq)
                {
                    minDistSq = distSq;
                    *actualPixel = pix;
                    *azError = eAz;
                    *altError = eAlt;
                }
            }
        }
    }
}

// Given a pixel, find its RA/DEC, then its alt/az, and then solve for another pixel
// where, if the star in pixel is moved to that star in the user's image (by adjusting alt and az controls)
// the polar alignment error would be 0.
bool PolarAlign::findCorrectedPixel(const QSharedPointer<FITSData> &image, const QPointF &pixel, QPointF *corrected,
                                    bool altOnly)
{
    double azOffset, altOffset;
    calculateAzAltError(&azOffset, &altOffset);
    if (altOnly)
        azOffset = 0.0;
    return findCorrectedPixel(image, pixel, corrected, azOffset, altOffset);
}

// Given a pixel, find its RA/DEC, then its alt/az, and then solve for another pixel
// where, if the star in pixel is moved to that star in the user's image (by adjusting alt and az controls)
// the polar alignment error would be 0. We use the fact that we can only move by adjusting and altitude
// knob, then an azimuth knob--i.e. we likely don't traverse a great circle.
bool PolarAlign::findCorrectedPixel(const QSharedPointer<FITSData> &image, const QPointF &pixel, QPointF *corrected,
                                    double azOffset,
                                    double altOffset)
{
    // 1. Find the az/alt for the x,y point on the image.
    SkyPoint p;
    if (!prepareAzAlt(image, pixel, &p))
        return false;
    double pixelAz = p.az().Degrees(), pixelAlt = p.alt().Degrees();

    // 2. Apply the az/alt offsets.
    // We know that the pole's az and alt offsets are effectively rotations
    // of a sphere. The offsets that apply to correct different points depend
    // on where (in the sphere) those points are. Points close to the pole can probably
    // just add the pole's offsets. This calculation is a bit more precise, and is
    // necessary if the points are not near the pole.
    double altRotation = northernHemisphere() ? altOffset : -altOffset;
    QPointF rotated = Rotations::rotateRaAxis(QPointF(pixelAz, pixelAlt), QPointF(azOffset, altRotation));

    // 3. Find a pixel with those alt/az values.
    if (!findAzAlt(image, rotated.x(), rotated.y(), corrected))
        return false;

    return true;
}
