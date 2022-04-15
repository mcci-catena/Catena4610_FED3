/*

Name:   Catena4610_FED3.h

Function:
    Global linkage for Catena4610_FED3.ino

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Dhinesh Kumar Pitchai, MCCI Corporation   May 2021

*/

#ifndef _Catena4610_FED3_h_
# define _Catena4610_FED3_h_

#pragma once

#include <Catena.h>
#include <Catena_Led.h>
#include <Catena_Mx25v8035f.h>
#include <Catena_Timer.h>
#include <SD.h>
#include <SPI.h>
#include "Catena4610_cMeasurementLoop.h"

// the global clock object

extern  McciCatena::Catena                      gCatena;
extern  McciCatena::cTimer                      ledTimer;
extern  McciCatena::Catena::LoRaWAN             gLoRaWAN;
extern  McciCatena::StatusLed                   gLed;

extern  SPIClass                                gSPI2;
extern  McciCatena4610::cMeasurementLoop        gMeasurementLoop;

//   The flash
extern  McciCatena::Catena_Mx25v8035f           gFlash;

#endif // !defined(_Catena4610_FED3_h_)
