// ---------------------------------------------------------------------------
// Flamingo Switch Library - v2.0
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

	static void decrypt(uint32_t input, uint16_t& receiverId, uint8_t& value, uint8_t& rollingCode, uint16_t& transmitterId);
	static uint32_t encrypt(uint8_t receiverId, uint8_t value, uint8_t rollingCode, uint16_t transmitterId);

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