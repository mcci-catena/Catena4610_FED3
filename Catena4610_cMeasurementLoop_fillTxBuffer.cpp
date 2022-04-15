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

        uint16_t fed3Vbat;
        fed3Vbat = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        if (fed3Vbat & 0x8000)
                fed3Vbat |= -0x10000;
        gCatena.SafePrintf("fed3Vbat: %d mV\n", (int) ((fed3Vbat / 4096.00) * 1000.f));

        uint32_t fed3NumMotorTurns;
        fed3NumMotorTurns = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3NumMotorTurns: %d\n", fed3NumMotorTurns);

        uint16_t fed3FixedRatio;
        fed3FixedRatio = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        if(fed3FixedRatio & 0x8000)
                fed3FixedRatio |= -0x10000;
        gCatena.SafePrintf("fed3FixedRatio: %d\n", fed3FixedRatio);

        uint16_t fed3EventActive;
        uint32_t fed3PokeTime;
        uint32_t fed3RetrievalTime;
        fed3EventActive = 0x03 & m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        if (fed3EventActive == 1)
                {
                gCatena.SafePrintf("fed3EventActive: Left\n");
                fed3PokeTime = ((m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]) >> 2);
                fed3PokeTime = fed3PokeTime & 0x3FFF;
                gCatena.SafePrintf("fed3PokeTime: %d ms\n", fed3PokeTime * 4);
                }
        else if (fed3EventActive == 2)
                {
                gCatena.SafePrintf("fed3EventActive: Right\n");
                fed3PokeTime = ((m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]) >> 2);
                fed3PokeTime = fed3PokeTime & 0x3FFF;
                gCatena.SafePrintf("fed3PokeTime: %d ms\n", fed3PokeTime * 4);
                }
        else if (fed3EventActive == 3)
                {
                gCatena.SafePrintf("fed3EventActive: Pellet\n");
                fed3RetrievalTime = ((m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]) >> 2);
                fed3RetrievalTime = fed3RetrievalTime & 0x3FFF;
                gCatena.SafePrintf("fed3RetrievalTime: %d ms\n", fed3RetrievalTime * 4);
                }
        else
                gCatena.SafePrintf("fed3EventActive: Unknown\n");
        dataIndex = dataIndex + 2;

        uint32_t fed3LeftCount;
        fed3LeftCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3LeftCount: %d\n", fed3LeftCount);

        uint32_t fed3RighttCount;
        fed3RighttCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3RighttCount: %d\n", fed3RighttCount);

        uint32_t fed3PelletCount;
        fed3PelletCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1]
                | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 2] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 3];
        dataIndex = dataIndex + 4;
        gCatena.SafePrintf("fed3PelletCount: %d\n", fed3PelletCount);

        uint16_t fed3BlockPelletCount;
        fed3BlockPelletCount = m_data.fed3.DataBytes[m_BufferIndex][dataIndex] << 8 | m_data.fed3.DataBytes[m_BufferIndex][dataIndex + 1];
        dataIndex = dataIndex + 2;
        if(fed3BlockPelletCount & 0x8000)
                fed3BlockPelletCount |= -0x10000;
        gCatena.SafePrintf("fed3BlockPelletCount: %d\n", fed3BlockPelletCount);
        }

    gLed.Set(McciCatena::LedPattern::Off);
    }
