/*  Ekos
    Copyright (C) 2012 Jasem Mutlaq <mutlaqja@ikarustech.com>

    This application is free software; you can redistribute it and/or
    modify it under the terms of the GNU General Public
    License as published by the Free Software Foundation; either
    version 2 of the License, or (at your option) any later version.
 */

#include <libindi/basedevice.h>

#include "ekosmanager.h"
//#include "capture.h"
#include "kstars.h"

#include <KMessageBox>
#include <KComboBox>

#include <config-kstars.h>

#include "indi/clientmanager.h"
#include "indi/indielement.h"
#include "indi/indiproperty.h"
#include "indi/driverinfo.h"
#include "indi/drivermanager.h"
#include "indi/indilistener.h"
#include "indi/guimanager.h"

EkosManager::EkosManager()
        : QDialog(KStars::Instance())
{
    setupUi(this);

    nDevices=0;
    useGuiderFromCCD=true;
    useFilterFromCCD=true;

    scope   =  NULL;
    ccd     =  NULL;
    guider  =  NULL;
    focuser =  NULL;
    filter  =  NULL;

    scope_di   = NULL;
    ccd_di     = NULL;
    guider_di  = NULL;
    focuser_di = NULL;
    filter_di  = NULL;

    captureProcess = NULL;
    focusProcess   = NULL;

    telescopeCombo->addItem("--");
    ccdCombo->addItem("--");
    guiderCombo->addItem("--");
    focuserCombo->addItem("--");
    filterCombo->addItem("--");

    foreach(DriverInfo *dv, DriverManager::Instance()->getDrivers())
    {
        switch (dv->getType())
        {
        case KSTARS_TELESCOPE:
             telescopeCombo->addItem(dv->getTreeLabel());
             break;

        case KSTARS_CCD:
            ccdCombo->addItem(dv->getTreeLabel());
            guiderCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_FOCUSER:
            focuserCombo->addItem(dv->getTreeLabel());
            break;

        case KSTARS_FILTER:
            filterCombo->addItem(dv->getTreeLabel());
            break;

        default:
            continue;
            break;

        }

        driversList[dv->getTreeLabel()] = dv;
    }

    toolsWidget->setTabEnabled(1, false);
    toolsWidget->setTabEnabled(2, false);
    toolsWidget->setTabEnabled(3, false);
    toolsWidget->setTabEnabled(4, false);

    connect(processINDIB, SIGNAL(clicked()), this, SLOT(processINDI()));

    connect(connectB, SIGNAL(clicked()), this, SLOT(connectDevices()));

    connect(disconnectB, SIGNAL(clicked()), this, SLOT(disconnectDevices()));

    connect(controlPanelB, SIGNAL(clicked()), GUIManager::Instance(), SLOT(show()));

}

EkosManager::~EkosManager() {}

void EkosManager::processINDI()
{
    if (managedDevices.count() > 0)
    {
        cleanDevices();
        return;
    }

    nDevices=0;
    managedDevices.clear();

    scope_di   = driversList.value(telescopeCombo->currentText());
    ccd_di     = driversList.value(ccdCombo->currentText());
    guider_di  = driversList.value(guiderCombo->currentText());
    filter_di  = driversList.value(filterCombo->currentText());
    focuser_di = driversList.value(focuserCombo->currentText());

    if (guider_di && guider_di != ccd_di)
        useGuiderFromCCD = false;
    else
        guider_di = NULL;

    if (filter_di && filter_di != ccd_di)
        useFilterFromCCD = false;
    else
        filter_di = NULL;

    if (scope_di != NULL)
        managedDevices.append(scope_di);
    if (ccd_di != NULL)
        managedDevices.append(ccd_di);
    if (guider_di != NULL)
        managedDevices.append(guider_di);
    if (filter_di != NULL)
        managedDevices.append(filter_di);

    if (focuser_di != NULL)
        managedDevices.append(focuser_di);

    if (managedDevices.empty()) return;

    nDevices = managedDevices.count();

    if (DriverManager::Instance()->startDevices(managedDevices) == false)
        return;

    connect(INDIListener::Instance(), SIGNAL(newDevice(ISD::GDInterface*)), this, SLOT(processNewDevice(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newTelescope(ISD::GDInterface*)), this, SLOT(setTelescope(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newCCD(ISD::GDInterface*)), this, SLOT(setCCD(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFilter(ISD::GDInterface*)), this, SLOT(setFilter(ISD::GDInterface*)));
    connect(INDIListener::Instance(), SIGNAL(newFocuser(ISD::GDInterface*)), this, SLOT(setFocuser(ISD::GDInterface*)));

    connect(INDIListener::Instance(), SIGNAL(deviceRemoved(ISD::GDInterface*)), this, SLOT(removeDevice(ISD::GDInterface*)));




    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);

    processINDIB->setText(i18n("Stop INDI"));
}

void EkosManager::connectDevices()
{
    if (scope)
         scope->runCommand(INDI_CONNECT);

    if (ccd)
        ccd->runCommand(INDI_CONNECT);

    if (guider)
        guider->runCommand(INDI_CONNECT);

    if (filter)
        filter->runCommand(INDI_CONNECT);

    if (focuser)
        focuser->runCommand(INDI_CONNECT);

    connectB->setEnabled(false);
    disconnectB->setEnabled(true);
}

void EkosManager::disconnectDevices()
{

    if (scope)
         scope->runCommand(INDI_DISCONNECT);

    if (ccd)
        ccd->runCommand(INDI_DISCONNECT);

    if (guider)
        guider->runCommand(INDI_DISCONNECT);

    if (filter)
        filter->runCommand(INDI_DISCONNECT);

    if (focuser)
        focuser->runCommand(INDI_DISCONNECT);

    connectB->setEnabled(true);
    disconnectB->setEnabled(false);

}

void EkosManager::cleanDevices()
{

    DriverManager::Instance()->stopDevices(managedDevices);

    nDevices = 0;
    managedDevices.clear();

    processINDIB->setText(i18n("Start INDI"));
    processINDIB->setEnabled(true);
    connectB->setEnabled(false);
    disconnectB->setEnabled(false);
    controlPanelB->setEnabled(false);
}

void EkosManager::processNewDevice(ISD::GDInterface *devInterface)
{
    foreach(DriverInfo *di, driversList.values())
    {
        if (di == devInterface->getDriverInfo())
        {
            switch (di->getType())
            {
               case KSTARS_TELESCOPE:
                scope = devInterface;
                break;


               case KSTARS_CCD:
                if (guider_di == di)
                    guider = devInterface;
                else
                    ccd = devInterface;

                break;

            case KSTARS_FOCUSER:
                focuser = devInterface;
                break;

             case KSTARS_FILTER:
                if (filter_di == di)
                    filter = devInterface;
                break;

              default:
                break;
            }

            nDevices--;
            break;
        }
    }

    if (nDevices == 0)
    {
        connectB->setEnabled(true);
        disconnectB->setEnabled(false);
        controlPanelB->setEnabled(true);
    }

}

void EkosManager::setTelescope(ISD::GDInterface *scopeDevice)
{
    //qDebug() << "Received set telescope scope device " << endl;
    scope = scopeDevice;
}

void EkosManager::setCCD(ISD::GDInterface *ccdDevice)
{
    //qDebug() << "Received set CCD device " << endl;

    if (useGuiderFromCCD == false)
        guider = ccdDevice;
    else
        ccd = ccdDevice;

    if (captureProcess == NULL)
    {
         captureProcess = new Ekos::Capture();
         toolsWidget->addTab( captureProcess, i18n("CCD"));
    }

    captureProcess->addCCD(ccdDevice);

    if (focusProcess != NULL)
        focusProcess->addCCD(ccd);


}

void EkosManager::setFilter(ISD::GDInterface *filterDevice)
{
    if (useFilterFromCCD == false)
       filter = filterDevice;
    else
        filter = ccd;

    if (captureProcess == NULL)
    {
         captureProcess = new Ekos::Capture();
         toolsWidget->addTab( captureProcess, i18n("CCD"));
    }

    captureProcess->addFilter(filter);
}

void EkosManager::setFocuser(ISD::GDInterface *focuserDevice)
{
    //qDebug() << "Received set focuser device " << endl;
    focuser = focuserDevice;

    if (focusProcess == NULL)
    {
         focusProcess = new Ekos::Focus();
         toolsWidget->addTab( focusProcess, i18n("Focus"));
    }

    focusProcess->addFocuser(focuser);

    if (ccd != NULL)
        focusProcess->addCCD(ccd);
}

void EkosManager::removeDevice(ISD::GDInterface* devInterface)
{
    switch (devInterface->getType())
    {
        case KSTARS_CCD:
        if (devInterface == ccd)
        {
            for (int i=0; i < toolsWidget->count(); i++)
                if (i18n("CCD") == toolsWidget->tabText(i))
                    toolsWidget->removeTab(i);

            ccd = NULL;
            delete (captureProcess);
            captureProcess = NULL;
        }

        break;

    case KSTARS_FOCUSER:
    if (devInterface == focuser)
    {
        for (int i=0; i < toolsWidget->count(); i++)
            if (i18n("Focus") == toolsWidget->tabText(i))
                toolsWidget->removeTab(i);

        focuser = NULL;
        delete (focusProcess);
        focusProcess = NULL;
    }

    break;

      default:
         break;
    }


    foreach(DriverInfo *drvInfo, managedDevices)
    {
        if (drvInfo == devInterface->getDriverInfo())
        {
            managedDevices.removeOne(drvInfo);

            if (managedDevices.count() == 0)
            {
                processINDIB->setText(i18n("Start INDI"));
                connectB->setEnabled(false);
                disconnectB->setEnabled(false);
                controlPanelB->setEnabled(false);
            }

            break;
        }
    }



}


#include "ekosmanager.moc"
