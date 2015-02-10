// ---------------------------------------------------------------------------
// Flamingo Switch Library - v1.0 - 10.2.2015
//
// AUTHOR/LICENSE:
// Created by Karl-Heinz Wind - karl-heinz.wind@web.de
// Copyright 2015 License: GNU GPL v3 http://www.gnu.org/licenses/gpl-3.0.html
// ---------------------------------------------------------------------------

#ifndef FLAMINGO_SWITCH_H
#define FLAMINGO_SWITCH_H

#include "Arduino.h"

// Number of maximum High/Low changes per packet.
// We can handle up to (unsigned long) => 32 bit * 2 H/L changes per bit + 2 for sync + timing[0]
// 32Bit-->67
#define FLAMINGO_MAX_CHANGES 32 * 2 + 2 + 1 

const unsigned int DEFAULT_RETRIES = 4;

class FlamingoSwitch
{

public:
	FlamingoSwitch();
	
	void enableTransmit(int nTransmitterPin);
	void disableTransmit();
	void transmit(int nHighPulses, int nLowPulses);

	bool available() const;
	void resetAvailable();

	unsigned long getReceivedValue() const;
	unsigned int getReceivedBitlength() const;
	unsigned int getReceivedDelay() const;
	unsigned int* getReceivedRawdata() const;

	void enableReceive(int interrupt);
	void enableReceive();
	void disableReceive();

	void send(uint32_t code, unsigned int retries = DEFAULT_RETRIES);
private:

	void send0();
	void send1();
	void sendSync();

	static void handleInterrupt();
	static bool receiveProtocol(unsigned int changeCount);

	static int nReceiveTolerance;
	static unsigned long nReceivedValue;
	static unsigned int nReceivedBitlength;
	static unsigned int nReceivedDelay;

	/*
	* timings[0] contains sync timing, followed by a number of bits
	*/
	static unsigned int timings[FLAMINGO_MAX_CHANGES];

	int nTransmitterPin;
	int nPulseLength;

	int nReceiverInterrupt;
};

#endif