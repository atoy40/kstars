/*
  Copyright (C) 2021, Hy Murveit

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
*/

#include "opsterrain.h"

#include "ksfilereader.h"
#include "kstars.h"
#include "kstarsdata.h"
#include "Options.h"
#include "skymap.h"
#include "skycomponents/skymapcomposite.h"
#include "auxiliary/kspaths.h"

#include <KConfigDialog>

#include <QPushButton>
#include <QFileDialog>

OpsTerrain::OpsTerrain() : QFrame(KStars::Instance())
{
    setupUi(this);

    m_ConfigDialog = KConfigDialog::exists("settings");

    connect(m_ConfigDialog->button(QDialogButtonBox::Apply), SIGNAL(clicked()), SLOT(slotApply()));
    connect(m_ConfigDialog->button(QDialogButtonBox::Ok), SIGNAL(clicked()), SLOT(slotApply()));

    selectTerrainFileB->setIcon(QIcon::fromTheme("document-open"));
    selectTerrainFileB->setAttribute(Qt::WA_LayoutUsesWidgetRect);
    connect(selectTerrainFileB, SIGNAL(clicked()), this, SLOT(saveTerrainFilename()));
}

void OpsTerrain::syncOptions()
{
    kcfg_ShowTerrain->setChecked(Options::showTerrain());
    kcfg_TerrainPanning->setChecked(Options::terrainPanning());
    kcfg_TerrainSkipSpeedup->setChecked(Options::terrainSkipSpeedup());
    kcfg_TerrainSmoothPixels->setChecked(Options::terrainSmoothPixels());
    kcfg_TerrainTransparencySpeedup->setChecked(Options::terrainTransparencySpeedup());
}


void OpsTerrain::saveTerrainFilename()
{
    QDir dir = QDir(KSPaths::writableLocation(QStandardPaths::AppDataLocation) + "/terrain");
    dir.mkpath(".");

    QUrl dirPath = QUrl::fromLocalFile(dir.path());
    QUrl fileUrl =
        QFileDialog::getOpenFileUrl(KStars::Instance(), i18nc("@title:window", "Terrain Image Filename"), dirPath,
                                    i18n("PNG Files (*.png)"));

    if (!fileUrl.isEmpty())
        kcfg_TerrainSource->setText(fileUrl.toLocalFile());
}

void OpsTerrain::slotApply()
{
    KStarsData *data = KStarsData::Instance();
    SkyMap *map      = SkyMap::Instance();

    data->setFullTimeUpdate();
    KStars::Instance()->updateTime();
    map->forceUpdate();
}
