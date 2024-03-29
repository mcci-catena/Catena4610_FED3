/*

Module: Catena4430_cMeasurementLoop.cpp

Function:
    Class for transmitting accumulated measurements.

Copyright:
    See accompanying LICENSE file for copyright and license information.

Author:
    Dhinesh Kumar Pitchai, MCCI Corporation   May 2021

*/

#include "Catena4610_cMeasurementLoop.h"
#include <arduino_lmic.h>
#include <Catena4610_FED3.h>
#include <Catena_Si1133.h>

using namespace McciCatena4610;
using namespace McciCatena;

extern cMeasurementLoop gMeasurementLoop;

static constexpr uint8_t kVddPin = D11;

/****************************************************************************\
|
|   An object to represent the uplink activity
|
\****************************************************************************/

void cMeasurementLoop::begin()
    {
    // register for polling.
    if (! this->m_registered)
        {
        this->m_registered = true;

        gCatena.registerObject(this);

        this->m_UplinkTimer.begin(this->m_txCycleSec * 1000);
        }

    Wire.begin();
    if (this->m_BME280.begin(BME280_ADDRESS, Adafruit_BME280::OPERATING_MODE::Sleep))
        {
        this->m_fBme280 = true;
        }
    else
        {
        this->m_fBme280 = false;
        gCatena.SafePrintf("No BME280 found: check wiring\n");
        }

    if (this->m_si1133.begin())
        {
        this->m_fSi1133 = true;

        auto const measConfig =	Catena_Si1133::ChannelConfiguration_t()
            .setAdcMux(Catena_Si1133::InputLed_t::LargeWhite)
            .setSwGainCode(7)
            .setHwGainCode(4)
            .setPostShift(1)
            .set24bit(true);

        this->m_si1133.configure(0, measConfig, 0);
        }
    else
        {
        this->m_fSi1133 = false;
        gCatena.SafePrintf("No Si1133 found: check hardware\n");
        }

    m_prevEvent = 0;
    m_BufferIndex = 0;

    // start (or restart) the FSM.
    if (! this->m_running)
        {
        this->m_exit = false;
        this->m_fsm.init(*this, &cMeasurementLoop::fsmDispatch);
        }

    this->u16InCnt = 0;
    this->u16errCnt = 0;
    this->u8BufferSize = 0;
    this->lastRec = 0;
    }

void cMeasurementLoop::end()
    {
    if (this->m_running)
        {
        this->m_exit = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::requestActive(bool fEnable)
    {
    if (fEnable)
        this->m_rqActive = true;
    else
        this->m_rqInactive = true;

    this->m_fsm.eval();
    }

cMeasurementLoop::State
cMeasurementLoop::fsmDispatch(
    cMeasurementLoop::State currentState,
    bool fEntry
    )
    {
    State newState = State::stNoChange;

    if (fEntry && this->isTraceEnabled(this->DebugFlags::kTrace))
        {
        gCatena.SafePrintf("cMeasurementLoop::fsmDispatch: enter %s\n",
                this->getStateName(currentState)
                );
        }

    switch (currentState)
        {
    case State::stInitial:
        newState = State::stInactive;
        this->resetMeasurements();
        break;

    case State::stInactive:
        if (fEntry)
            {
            // turn off anything that should be off while idling.
            }
        if (this->m_rqActive)
            {
            // when going active manually, start the measurement
            // cycle immediately.
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = true;
            this->m_UplinkTimer.retrigger();
            newState = State::stWarmup;
            }
        break;

    case State::stSleeping:
        if (fEntry)
            {
            // set the LEDs to flash accordingly.
            gLed.Set(McciCatena::LedPattern::Sleeping);
            }

        if (this->m_rqInactive)
            {
            this->m_rqActive = this->m_rqInactive = false;
            this->m_active = false;
            newState = State::stInactive;
            }
        else if (this->m_UplinkTimer.isready())
            newState = State::stMeasure;
        else if (this->m_UplinkTimer.getRemaining() > 1500)
            this->sleep();
        break;

    // get some data. This is only called while booting up.
    case State::stWarmup:
        if (fEntry)
            {
            //start the timer
            this->setTimer(5 * 1000);
            }

        if (this->timedOut())
            newState = State::stMeasure;
        break;

    // fill in the measurement
    case State::stMeasure:
        if (fEntry)
            {
            // start SI1133 measurement (one-time)
            this->m_si1133.start(true);
            this->updateSynchronousMeasurements();
            this->setTimer(1000);
            }

        if (this->m_si1133.isOneTimeReady())
            {
            // this->updateLightMeasurements();
            this->m_si1133.stop();
            newState = State::stTransmit;
            }
        else if (this->timedOut())
            {
            this->m_si1133.stop();
            newState = State::stTransmit;
            if (this->isTraceEnabled(this->DebugFlags::kError))
                gCatena.SafePrintf("S1133 timed out\n");
            }
        break;

    case State::stTransmit:
        if (fEntry)
            {
            TxBuffer_t b;

            this->fillTxBuffer(b, this->m_data);
            this->m_FileData = this->m_data;

            this->m_FileTxBuffer.begin();
            for (auto i = 0; i < b.getn(); ++i)
                this->m_FileTxBuffer.put(b.getbase()[i]);

            if (gLoRaWAN.IsProvisioned())
                this->startTransmission(b);

            m_BufferIndex = m_BufferIndex + 1;
            }
        if (! gLoRaWAN.IsProvisioned())
            {
            newState = State::stSleeping;
            }
        if (this->txComplete())
            {
            if (m_BufferIndex < m_eventCount)
                newState = State::stMeasure;

            else
                {
                newState = State::stSleeping;

                // calculate the new sleep interval.
                this->updateTxCycleTime();
                this->resetMeasurements();
                }
            }
        break;

    case State::stFinal:
        break;

    default:
        break;
        }

    return newState;
    }

/****************************************************************************\
|
|   Take a measurement
|
\****************************************************************************/

void cMeasurementLoop::resetMeasurements()
    {
    memset((void *) &this->m_data, 0, sizeof(this->m_data));
    this->m_data.flags = Flags(0);
    m_eventCount = 0;
    m_BufferIndex = 0;
    }

void cMeasurementLoop::updateSynchronousMeasurements()
    {
    this->m_data.Vbat = gCatena.ReadVbat();
    this->m_data.flags |= Flags::Vbat;

    this->m_data.Vbus = gCatena.ReadVbus();
    this->m_data.flags |= Flags::Vbus;

    if (gCatena.getBootCount(this->m_data.BootCount))
        {
        this->m_data.flags |= Flags::Boot;
        }

    if (this->m_fBme280)
        {
        auto m = this->m_BME280.readTemperaturePressureHumidity();
        this->m_data.env.Temperature = m.Temperature;
        this->m_data.env.Pressure = m.Pressure;
        this->m_data.env.Humidity = m.Humidity;
        this->m_data.flags |= Flags::TPH;
        }

    }

void cMeasurementLoop::updateLightMeasurements()
    {
    uint32_t data[1];

    this->m_si1133.readMultiChannelData(data, 1);
    this->m_si1133.stop();

    this->m_data.flags |= Flags::Light;
    this->m_data.light.White = (float) data[0];
    }

void cMeasurementLoop::updatePelletFeederData()
    {
    uint8_t current;

    // check if there is any incoming frame
    current = Serial1.available();

    if (current == 0)
        return;
    if (current != this->lastRec)
        {
        this->lastRec = current;
        this->u32time = millis() + kT35;
        return;
        }
    if ((int32_t)(millis() - this->u32time) < 0)
        {
        return;
        }

    this->lastRec = 0;

    this->num_bytes = this->getRxBuffer(this->errCode);

    if (this->errCode == SUCCESS)
        {
        this->errCode = this->validateAnswer();
        }

    if (this->errCode == SUCCESS)        
        {
        if (m_eventCount > 9)
            {
            for (uint8_t nIndex = 0; nIndex <= this->num_bytes; ++nIndex)
                {
                m_data.fed3.DataBytes[m_eventCount - 10][nIndex] = m_data.fed3.DataBytes[m_eventCount - 9][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 9][nIndex] = m_data.fed3.DataBytes[m_eventCount - 8][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 8][nIndex] = m_data.fed3.DataBytes[m_eventCount - 7][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 7][nIndex] = m_data.fed3.DataBytes[m_eventCount - 6][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 6][nIndex] = m_data.fed3.DataBytes[m_eventCount - 5][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 5][nIndex] = m_data.fed3.DataBytes[m_eventCount - 4][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 4][nIndex] = m_data.fed3.DataBytes[m_eventCount - 3][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 3][nIndex] = m_data.fed3.DataBytes[m_eventCount - 2][nIndex];
                m_data.fed3.DataBytes[m_eventCount - 2][nIndex] = m_data.fed3.DataBytes[m_eventCount - 1][nIndex];
                }
            m_eventCount = 9;
            }
        for (uint8_t nIndex = 1; nIndex <= this->num_bytes; ++nIndex)
            {
            m_data.fed3.DataBytes[m_eventCount][nIndex - 1] = this->au8Buffer[nIndex + BYTE_CNT];
            }
        if (m_eventCount <= 9)
            {
            m_eventCount += 1;
            }
        this->m_data.flags |= Flags::FED3;
        }
    }

uint8_t cMeasurementLoop::getRxBuffer(uint8_t &errcode)
{
    boolean bBuffOverflow = false;

    this->u8BufferSize = 0;
    while ( Serial1.available() )
        {
        if (this->u8BufferSize >= sizeof(this->au8Buffer))
            bBuffOverflow = true;
        else
            {
            this->au8Buffer[ this->u8BufferSize++ ] = Serial1.read();
            }
        }
    this->u16InCnt++;

    if (bBuffOverflow)
    {
        this->u16errCnt++;
        errcode = BUFF_OVERFLOW;
    }
    else
    {
        errcode = SUCCESS;
    }
    return this->u8BufferSize;
}

uint16_t cMeasurementLoop::getMessageWord(unsigned offset)
        {
        return (this->au8Buffer[offset] << 8u) + this->au8Buffer[offset + 1];
        }

uint16_t cMeasurementLoop::calcCRC(uint8_t u8length)
{
    unsigned int temp, temp2, flag;
    temp = 0xFFFF;
    for (unsigned char i = 0; i < u8length; i++)
    {
        temp = temp ^ this->au8Buffer[i];
        for (unsigned char j = 1; j <= 8; j++)
        {
            flag = temp & 0x0001;
            temp >>=1;
            if (flag)
                temp ^= 0xA001;
        }
    }
    // Reverse byte order.
    temp2 = temp >> 8;
    temp = (temp << 8) | temp2;
    temp &= 0xFFFF;
    // the returned value is already swapped
    // crcLo byte is first & crcHi byte is last
    return temp;
}

uint8_t cMeasurementLoop::validateAnswer()
    {
    if (this->u8BufferSize < 4)
    {
        this->u16errCnt ++;
        return RUNT_PACKET;
    }
    // check message crc vs calculated crc
    uint16_t u16MsgCRC = this->getMessageWord(this->u8BufferSize - 2);

    if ( this->calcCRC( this->u8BufferSize-2 ) != u16MsgCRC )
        {
        this->u16errCnt ++;
        gCatena.SafePrintf("CRC: %d\n", this->au8Buffer[unsigned(SerialMessageOffset::ID)]);
        return BAD_CRC;
        }

    // check exception
    if (this->au8Buffer[ unsigned(SerialMessageOffset::ID) ] != 0x01)
        {
        this->u16errCnt ++;
        gCatena.SafePrintf("Message ID: %d\n", this->au8Buffer[unsigned(SerialMessageOffset::ID)]);
        return INVALID_MSG_ID;
        }

    return SUCCESS; // OK, no exception code thrown
    }

/****************************************************************************\
|
|   Start uplink of data
|
\****************************************************************************/

void cMeasurementLoop::startTransmission(
    cMeasurementLoop::TxBuffer_t &b
    )
    {
    gLed.Set(McciCatena::LedPattern::Off);
    gLed.Set(McciCatena::LedPattern::Sending);

    // by using a lambda, we can access the private contents
    auto sendBufferDoneCb =
        [](void *pClientData, bool fSuccess)
            {
            auto const pThis = (cMeasurementLoop *)pClientData;
            pThis->m_txpending = false;
            pThis->m_txcomplete = true;
            pThis->m_txerr = ! fSuccess;
            pThis->m_fsm.eval();
            };

    bool fConfirmed = false;
    if (gCatena.GetOperatingFlags() &
        static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fConfirmedUplink))
        {
        gCatena.SafePrintf("requesting confirmed tx\n");
        fConfirmed = true;
        }

    this->m_txpending = true;
    this->m_txcomplete = this->m_txerr = false;

    if (! gLoRaWAN.SendBuffer(b.getbase(), b.getn(), sendBufferDoneCb, (void *)this, fConfirmed, kUplinkPort))
        {
        // uplink wasn't launched.
        this->m_txcomplete = true;
        this->m_txerr = true;
        this->m_fsm.eval();
        }
    }

void cMeasurementLoop::sendBufferDone(bool fSuccess)
    {
    this->m_txpending = false;
    this->m_txcomplete = true;
    this->m_txerr = ! fSuccess;
    this->m_fsm.eval();
    }

/****************************************************************************\
|
|   The Polling function --
|
\****************************************************************************/

void cMeasurementLoop::poll()
    {
    bool fEvent;

    // no need to evaluate unless something happens.
    fEvent = false;

    // if we're not active, and no request, nothing to do.
    if (! this->m_active)
        {
        if (! this->m_rqActive)
            return;

        // we're asked to go active. We'll want to eval.
        fEvent = true;
        }

    this->updatePelletFeederData();

    if (this->m_fTimerActive)
        {
        if ((millis() - this->m_timer_start) >= this->m_timer_delay)
            {
            this->m_fTimerActive = false;
            this->m_fTimerEvent = true;
            fEvent = true;
            }
        }

    // check the transmit time.
    if (this->m_UplinkTimer.peekTicks() != 0)
        {
        fEvent = true;
        }

    if (fEvent)
        this->m_fsm.eval();

    this->m_data.Vbus = gCatena.ReadVbus();
    setVbus(this->m_data.Vbus);
    }

/****************************************************************************\
|
|   Update the TxCycle count.
|
\****************************************************************************/

void cMeasurementLoop::updateTxCycleTime()
    {
    auto txCycleCount = this->m_txCycleCount;

    // update the sleep parameters
    if (txCycleCount > 1)
            {
            // values greater than one are decremented and ultimately reset to default.
            this->m_txCycleCount = txCycleCount - 1;
            }
    else if (txCycleCount == 1)
            {
            // it's now one (otherwise we couldn't be here.)
            gCatena.SafePrintf("resetting tx cycle to default: %u\n", this->m_txCycleSec_Permanent);

            this->setTxCycleTime(this->m_txCycleSec_Permanent, 0);
            }
    else
            {
            // it's zero. Leave it alone.
            }
    }

/****************************************************************************\
|
|   Handle sleep between measurements
|
\****************************************************************************/

void cMeasurementLoop::sleep()
    {
    const bool fDeepSleep = checkDeepSleep();

    if (! this->m_fPrintedSleeping)
            this->doSleepAlert(fDeepSleep);

    if (fDeepSleep)
            this->doDeepSleep();
    }

// for now, we simply don't allow deep sleep. In the future might want to
// use interrupts on activity to wake us up; then go back to sleep when we've
// seen nothing for a while.
bool cMeasurementLoop::checkDeepSleep()
    {
    bool const fDeepSleepTest = gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
    bool fDeepSleep;
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (! this->kEnableDeepSleep)
        {
        return false;
        }

    if (sleepInterval < 2)
        fDeepSleep = false;
    else if (fDeepSleepTest)
        {
        fDeepSleep = true;
        }
#ifdef USBCON
    else if (Serial.dtr())
        {
        fDeepSleep = false;
        }
#endif
    else if (gCatena.GetOperatingFlags() &
                static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDisableDeepSleep))
        {
        fDeepSleep = false;
        }
    else if ((gCatena.GetOperatingFlags() &
                static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fUnattended)) != 0)
        {
        fDeepSleep = true;
        }
    else
        {
        fDeepSleep = false;
        }

    return fDeepSleep;
    }

void cMeasurementLoop::doSleepAlert(bool fDeepSleep)
    {
    this->m_fPrintedSleeping = true;

    if (fDeepSleep)
        {
        bool const fDeepSleepTest =
                gCatena.GetOperatingFlags() &
                    static_cast<uint32_t>(gCatena.OPERATING_FLAGS::fDeepSleepTest);
        const uint32_t deepSleepDelay = fDeepSleepTest ? 10 : 30;

        gCatena.SafePrintf("using deep sleep in %u secs"
#ifdef USBCON
                            " (USB will disconnect while asleep)"
#endif
                            ": ",
                            deepSleepDelay
                            );

        // sleep and print
        gLed.Set(McciCatena::LedPattern::TwoShort);

        for (auto n = deepSleepDelay; n > 0; --n)
            {
            uint32_t tNow = millis();

            while (uint32_t(millis() - tNow) < 1000)
                {
                gCatena.poll();
                yield();
                }
            gCatena.SafePrintf(".");
            }
        gCatena.SafePrintf("\nStarting deep sleep.\n");
        uint32_t tNow = millis();
        while (uint32_t(millis() - tNow) < 100)
            {
            gCatena.poll();
            yield();
            }
        }
    else
        gCatena.SafePrintf("using light sleep\n");
    }

void cMeasurementLoop::doDeepSleep()
    {
    std::uint32_t const sleepInterval = this->m_UplinkTimer.getRemaining() / 1000;

    if (sleepInterval == 0)
        return;

    /* ok... now it's time for a deep sleep */
    gLed.Set(McciCatena::LedPattern::Off);
    this->deepSleepPrepare();

    /* sleep */
    gCatena.Sleep(sleepInterval);

    /* recover from sleep */
    this->deepSleepRecovery();

    /* and now... we're awake again. trigger another measurement */
    this->m_fsm.eval();
    }

void cMeasurementLoop::deepSleepPrepare(void)
    {
    pinMode(kVddPin, INPUT);

    Serial.end();
    Wire.end();
    SPI.end();
    if (this->m_pSPI2 && this->m_fSpi2Active)
        {
        this->m_pSPI2->end();
        this->m_fSpi2Active = false;
        }
    }

void cMeasurementLoop::deepSleepRecovery(void)
    {
    pinMode(kVddPin, OUTPUT);
    digitalWrite(kVddPin, HIGH);
    
    Serial.begin();
    Wire.begin();
    SPI.begin();
    //if (this->m_pSPI2)
    //    this->m_pSPI2->begin();
    }

/****************************************************************************\
|
|  Time-out asynchronous measurements.
|
\****************************************************************************/

// set the timer
void cMeasurementLoop::setTimer(std::uint32_t ms)
    {
    this->m_timer_start = millis();
    this->m_timer_delay = ms;
    this->m_fTimerActive = true;
    this->m_fTimerEvent = false;
    }

void cMeasurementLoop::clearTimer()
    {
    this->m_fTimerActive = false;
    this->m_fTimerEvent = false;
    }

bool cMeasurementLoop::timedOut()
    {
    bool result = this->m_fTimerEvent;
    this->m_fTimerEvent = false;
    return result;
    }
