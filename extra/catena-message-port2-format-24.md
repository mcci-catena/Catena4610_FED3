# Understanding MCCI Catena data sent on port 2 format 0x22

<!-- markdownlint-disable MD033 -->
<!-- markdownlint-capture -->
<!-- markdownlint-disable -->
<!-- TOC depthFrom:2 updateOnSave:true -->

- [Overall Message Format](#overall-message-format)
- [Optional fields](#optional-fields)
	- [Battery Voltage (field 0)](#battery-voltage-field-0)
	- [System Voltage (field 1)](#system-voltage-field-1)
	- [Bus Voltage (field 2)](#bus-voltage-field-2)
	- [Boot counter (field 3)](#boot-counter-field-3)
	- [Environmental Readings (field 4)](#environmental-readings-field-4)
	- [Ambient Light (field 5)](#ambient-light-field-5)
	- [FED3 Data bytes (field 6)](#fed3-data-bytes-field-6)
		- [Battery voltage](#battery-voltage)
		- [Number of Mototr Turns](#num-motor-turns)
		- [Fixed Ratio](#fixed-ratio)
		- [Event Active and Time](#event-active-and-time)
		- [Left Count](#left-count)
		- [Right Count](#right-count)
		- [Pellet Count](#pellet-count)
		- [Block Pellet Count](#block-pellet-count)
- [Data Formats](#data-formats)
	- [`uint16`](#uint16)
	- [`int16`](#int16)
	- [`uint8`](#uint8)
	- [`uflt16`](#uflt16)
	- [`sflt16`](#sflt16)
	- [`uint32`](#uint32)

<!-- /TOC -->
<!-- markdownlint-restore -->
<!-- Due to a bug in Markdown TOC, the table is formatted incorrectly if tab indentation is set other than 4. Due to another bug, this comment must be *after* the TOC entry. -->
<!-- due to another bug in Markdown TOC, you need to have Editor>Tab Size set to 4 (even though it will be auto-overridden); it uses that setting rather than the current setting for the file. -->

## Overall Message Format

Port 2 format 0x24 uplink messages are sent by Catena4430_Sensor and related sketches. We use the discriminator byte in the same way as many of the sketches in the Catena-Sketches collection.

Each message has the following layout. There is a fixed part, followed by a variable part. THe maximum message size is 37 bytes, which allows for operation as slow as DR1 (SF9) in the United States.

byte | description
:---:|:---
0    | magic number 0x24
1    | a single byte, interpreted as a bit map indicating the fields that follow in bytes 2..*.
2..* | data bytes; use bitmap to map these bytes onto fields.

## Optional fields

Each bit in byte 1 represents whether a corresponding field in bytes 6..n is present. If all bits are clear, then no data bytes are present. If bit 0 is set, then field 0 is present; if bit 1 is set, then field 1 is present, and so forth. If a field is omitted, all bytes for that field are omitted.

The bitmap byte has the following interpretation. `int16`, `uint16`, etc. are defined after the table.

Bitmap bit | Length of corresponding field (bytes) | Data format |Description
:---:|:---:|:---:|:----
0 | 2 | [`int16`](#int16) | [Battery voltage](#battery-voltage-field-0)
1 | 2 | [`int16`](#int16) | [System voltage](#sys-voltage-field-1)
2 | 2 | [`int16`](#int16) | [Bus voltage](#bus-voltage-field-2)
3 | 1 | [`uint8`](#uint8) | [Boot counter](#boot-counter-field-3)
4 | 6 | [`int16`](#int16), [`uint16`](#uint16), [`uint16`](#uint16) | [Temperature, Pressure, Humidity](environmental-readings-field-4)
5 | 2 | [`uflt16`](#uflt16) | [Ambient Light](#ambient-light-field-5)
6 | 24 | [`uint8`](#uint8) | [FED3 Data bytes](#fed3-data-bytes-field-6)

### Battery Voltage (field 0)

Field 0, if present, carries the current battery voltage. To get the voltage, extract the int16 value, and divide by 4096.0. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### System Voltage (field 1)

Field 1, if present, carries the current System voltage. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

_Note:_ this field is not transmitted by some versions of the sketches.

### Bus Voltage (field 2)

Field 2, if present, carries the current voltage from USB VBus. Divide by 4096.0 to convert from counts to volts. (Thus, this field can represent values from -8.0 volts to 7.998 volts.)

### Boot counter (field 3)

Field 3, if present, is a counter of number of recorded system reboots, modulo 256.

### Environmental Readings (field 4)

Field 4, if present, has three environmental readings as six bytes of data.

- The first two bytes are a [`int16`](#int16) representing the temperature (divide by 256 to get degrees Celsius).

- The next two bytes are a [`uint16`](#uint16) representing the barometric pressure (divide by 25 to get millibars). This is the station pressure, not the sea-level pressure.

- The next two bytes are a [`uint16`](#uint16) representing the relative humidity (divide by 65535 to get percent). This field can represent humidity from 0% to 100%, in steps of roughly 0.001529%.

### Ambient Light (field 5)

This field represents the ambient "white light" expressed as W/cm^2. If the field is zero, it will not be transmitted.

### FED3 Data bytes (field 6)

Field six, if present, represents FED3 data bytes. This field has 13 bytes of data.

FED3 Data | Length of corresponding field (bytes) | Data format |Description
:---:|:---:|:---:|:----
1 | 2 | [`int16`](#int16) | [Battery voltage](#battery-voltage)
2 | 4 | [`uint32`](#uint32) | [Number of Mototr Turns](#num-motor-turns)
3 | 2 | [`int16`](#int16) | [Fixed Ratio](#fixed-ratio)
4 | 2 | [`uint16`](#uint16) | [Event Active and Time](#event-active-and-time)
5 | 4 | [`uint32`](#uint32) | [Left Count](#left-count)
6 | 4 | [`uint32`](#uint32) | [Right Count](#right-count)
7 | 4 | [`uint32`](#uint32) | [Pellet Count](#pellet-count)
8 | 2 | [`uint16`](#uint16) | [Block Pellet Count](#block-pellet-count)

#### Battery voltage

This is a 2-byte field, represents the Battery voltage of FED3 device.

#### Number of Motor Turns

This is a 4-byte field, represents the number of motor turns.

#### Fixed ratio

This is a 2-byte field, represnts the mode of the FED3 device.

#### Event Active

This is a 2-byte field, LSB two bits represents the last active poke event of FED3 device. And the other 14-bits represent the poke time for Left and Right poke (OR) retrieval time, the interval between pellet dispensing and being taken.

#### Left Count

This is a 4-byte field, represents the number of poke in left side of FED3 device.

#### Right Count

This is a 4-byte field, represents the number of poke in right side of FED3 device.

#### Pellet Count

This is a 4-byte field, represents the number of poke in pellet dispenser of FED3 device.

#### Block Pellet Count

This is a 2-byte field, represents the number of pellets dispensed of FED3 device.

## Data Formats

All multi-byte data is transmitted with the most significant byte first (big-endian format).  Comments on the individual formats follow.

### `uint16`

An integer from 0 to 65,535.

### `int16`

A signed integer from -32,768 to 32,767, in two's complement form. (Thus 0..0x7FFF represent 0 to 32,767; 0x8000 to 0xFFFF represent -32,768 to -1).

### `uint8`

An integer from 0 to 255.

### `uflt16`

An unsigned floating point number in the half-open range [0, 1), transmitted as a 16-bit number with the following interpretation:

bits | description
:---:|:---
15..12 | binary exponent `b`
11..0 | fraction `f`

The floating point number is computed by computing `f`/4096 * 2^(`b`-15). Note that this format is deliberately not IEEE-compliant; it's intended to be easy to decode by hand and not overwhelmingly sophisticated.

For example, if the transmitted message contains 0x1A, 0xAB, the equivalent floating point number is found as follows.

1. The full 16-bit number is 0x1AAB.
2. `b`  is therefore 0x1, and `b`-15 is -14.  2^-14 is 1/32768
3. `f` is 0xAAB. 0xAAB/4096 is 0.667
4. `f * 2^(b-15)` is therefore 0.6667/32768 or 0.0000204

Floating point mavens will immediately recognize:

* There is no sign bit; all numbers are positive.
* Numbers do not need to be normalized (although in practice they always are).
* The format is somewhat wasteful, because it explicitly transmits the most-significant bit of the fraction. (Most binary floating-point formats assume that `f` is is normalized, which means by definition that the exponent `b` is adjusted and `f` is shifted left until the most-significant bit of `f` is one. Most formats then choose to delete the most-significant bit from the encoding. If we were to do that, we would insist that the actual value of `f` be in the range 2048.. 4095, and then transmit only `f - 2048`, saving a bit. However, this complicated the handling of gradual underflow; see next point.)
* Gradual underflow at the bottom of the range is automatic and simple with this encoding; the more sophisticated schemes need extra logic (and extra testing) in order to provide the same feature.

### `sflt16`

A signed floating point number in the half-open range [0, 1), transmitted as a 16-bit number with the following interpretation:

bits | description
:---:|:---
15   | Sign (negative if set)
14..11 | binary exponent `b`
10..0 | fraction `f`

The floating point number is computed by computing `f`/2048 * 2^(`b`-15). Note that this format is deliberately not IEEE-compliant; it's intended to be easy to decode by hand and not overwhelmingly sophisticated.

Floating point mavens will immediately recognize:

* This is a sign/magnitude format, not a two's complement format.
* Numbers do not need to be normalized (although in practice they always are).
* The format is somewhat wasteful, because it explicitly transmits the most-significant bit of the fraction. (Most binary floating-point formats assume that `f` is is normalized, which means by definition that the exponent `b` is adjusted and `f` is shifted left until the most-significant bit of `f` is one. Most formats then choose to delete the most-significant bit from the encoding. If we were to do that, we would insist that the actual value of `f` be in the range 2048..4095, and then transmit only `f - 2048`, saving a bit. However, this complicates the handling of gradual underflow; see next point.)
* Gradual underflow at the bottom of the range is automatic and simple with this encoding; the more sophisticated schemes need extra logic (and extra testing) in order to provide the same feature.

### `uint32`

An integer from 0 to 4,294,967,295. The first byte is the most-significant 8 bits; the last byte is the least-significant 8 bits.

## The Things Network Console decoding script

The repository contains a generic script that decodes messages in this format, for [The Things Network console](https://console.thethingsnetwork.org).

## Node-RED Decoding Script

A Node-RED script to decode this data is part of this repository. You can download the latest version from GitHub:
