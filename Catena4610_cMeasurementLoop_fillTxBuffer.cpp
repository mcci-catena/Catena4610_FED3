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

using namespace McciCatena4610;

/*

Name:   McciCatena4430::cMeasurementLoop::fillTxBuffer()

Function:
    Prepare a messages in a TxBuffer with data from current measurements.

Definition:
    void McciCatena4430::cMeasurementLoop::fillTxBuffer(
            cMeasurementLoop::TxBuffer_t& b
            );

Description:
    A format 0x24 message is prepared from the data in the cMeasurementLoop
    object.

*/

void
cMeasurementLoop::fillTxBuffer(
    cMeasurementLoop::TxBuffer_t& b, Measurement const &mData
    )
    {
    gLed.Set(McciCatena::LedPattern::Off);
    gLed.Set(McciCatena::LedPattern::Measuring);

    // initialize the message buffer to an empty state
    b.begin();

    // insert format byte
    b.put(kMessageFormat);

    // the flags in Measurement correspond to the over-the-air flags.
    b.put(std::uint8_t(mData.flags));

    // send Vbat
    if ((mData.flags & Flags::Vbat) != Flags(0))
        {
        float Vbat = mData.Vbat;
        gCatena.SafePrintf("Vbat:    %d mV\n", (int) (Vbat * 1000.0f));
        b.putV(Vbat);
        }

    // Vbus is sent as 5000 * v
    if ((mData.flags & Flags::Vbus) != Flags(0))
        {
        float Vbus = mData.Vbus;
        gCatena.SafePrintf("Vbus:    %d mV\n", (int) (Vbus * 1000.0f));
        b.putV(Vbus);
        }

    // send boot count
    if ((mData.flags & Flags::Boot) != Flags(0))
        {
        b.putBootCountLsb(mData.BootCount);
        }

    if ((mData.flags & Flags::TPH) != Flags(0))
        {
        gCatena.SafePrintf(
                "BME280:  T: %d P: %d RH: %d\n",
                (int) mData.env.Temperature,
                (int) mData.env.Pressure,
                (int) mData.env.Humidity
                );
        b.putT(mData.env.Temperature);
        b.putP(mData.env.Pressure);
        // no method for 2-byte RH, directly encode it.
        b.put2uf((mData.env.Humidity / 100.0f) * 65535.0f);
        }

    // put light
    if ((mData.flags & Flags::Light) != Flags(0))
        {
        gCatena.SafePrintf(
                "Si1133:  %d White\n",
                (int) mData.light.White
                );

        b.putLux(LMIC_f2uflt16(mData.light.White / pow(2.0, 24)));
        }

    // put fed3 data
    if ((mData.flags & Flags::FED3) != Flags(0)){
        gCatena.SafePrintf("Data:");
        for (uint8_t nIndex = 0; nIndex < this->num_bytes; ++nIndex)
                {
                gCatena.SafePrintf(" %x", m_data.fed3.DataBytes[m_BufferIndex][nIndex]);
                b.put(m_data.fed3.DataBytes[m_BufferIndex][nIndex]);
                }
        gCatena.SafePrintf("\n");

        uint8_t dataIndex;
        dataIndex = 0;

        // Timestamp (4-bytes)
        uint32_t fed3TimeStamp;
        fed3TimeStamp = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 24 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1] << 16
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3TimeStamp: %u\n", fed3TimeStamp);

        // Version (3-bytes)
        uint32_t fed3VersionMajor;
        uint32_t fed3VersionMinor;
        uint32_t fed3VersionLocal;
        fed3VersionMajor = m_data.fed3.DataBytes[m_BufferIndex][dataIndex];
        dataIndex = dataIndex + 1;
        fed3VersionMinor = m_data.fed3.DataBytes[m_BufferIndex][dataIndex];
        dataIndex = dataIndex + 1;
        fed3VersionLocal = m_data.fed3.DataBytes[m_BufferIndex][dataIndex];
        dataIndex = dataIndex + 1;
        gCatena.SafePrintf("fed3Version: %d.%d.%d\n", fed3VersionMajor, fed3VersionMinor, fed3VersionLocal);

        // device number (2-bytes)
        uint16_t fed3DeviceNumber;
        fed3DeviceNumber = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        gCatena.SafePrintf("fed3DeviceNumber: %d\n", fed3DeviceNumber);

        // session Type (1-byte)
        uint16_t fed3SessionType;
        fed3SessionType = m_data.fed3.DataBytes[m_BufferIndex][dataIndex];
        dataIndex = dataIndex + 1;
        gCatena.SafePrintf("fed3SessionType Index: [%d] %s\n", fed3SessionType, sessionType[fed3SessionType]);

        // FED3 Battery voltage
		uint16_t fed3Vbat;
        fed3Vbat = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        if (fed3Vbat & 0x8000)
                fed3Vbat |= -0x10000;
        gCatena.SafePrintf("fed3Vbat: %d mV\n", (int) ((fed3Vbat / 4096.00) * 1000.f));

        // number of motor turns (4-bytes)
        uint32_t fed3NumMotorTurns;
        fed3NumMotorTurns = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 24 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1] << 16
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3NumMotorTurns: %d\n", fed3NumMotorTurns);

        // fixed ratio (2-bytes)
        uint16_t fed3FixedRatio;
        fed3FixedRatio = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        if(fed3FixedRatio & 0x8000)
                fed3FixedRatio |= -0x10000;
        gCatena.SafePrintf("fed3FixedRatio: %d\n", fed3FixedRatio);

        // Event Active (1-byte)
        uint16_t fed3EventActive;
        fed3EventActive = m_data.fed3.DataBytes[m_BufferIndex][dataIndex];
        dataIndex = dataIndex + 1;
        gCatena.SafePrintf("fed3EventActive Index: [%d] %s\n", fed3EventActive, eventActive[fed3EventActive]);

        // Event time (2-bytes)
        uint32_t fed3PokeTime;
        uint32_t fed3RetrievalTime;
        if (fed3EventActive == 11)
                {
                fed3RetrievalTime = ((m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]));
                gCatena.SafePrintf("fed3RetrievalTime: %d ms\n", fed3RetrievalTime * 4);
                }
        else
                {
                fed3PokeTime = ((m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]));
                gCatena.SafePrintf("fed3PokeTime: %d ms\n", fed3PokeTime * 4);
                }
        dataIndex = dataIndex + 2;

        // Left count (4-bytes)
        uint32_t fed3LeftCount;
        fed3LeftCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 24 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1] << 16
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3LeftCount: %d\n", fed3LeftCount);

        // Right count (4-bytes)
        uint32_t fed3RightCount;
        fed3RightCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 24 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1] << 16
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3RightCount: %d\n", fed3RightCount);

        // Pellet count (4-bytes)
        uint32_t fed3PelletCount;
        fed3PelletCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 24 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1] << 16
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3PelletCount: %d\n", fed3PelletCount);

        // Block pellet count (2-bytes)
		uint16_t fed3BlockPelletCount;
        fed3BlockPelletCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        if(fed3BlockPelletCount & 0x8000)
                fed3BlockPelletCount |= -0x10000;
        gCatena.SafePrintf("fed3BlockPelletCount: %d\n", fed3BlockPelletCount);
        }

    gLed.Set(McciCatena::LedPattern::Off);
    }
