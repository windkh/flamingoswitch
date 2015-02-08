#include "FlamingoSwitch.h"


unsigned long FlamingoSwitch::nReceivedValue = NULL;
unsigned int FlamingoSwitch::nReceivedBitlength = 0;
int FlamingoSwitch::nReceiveTolerance = 60;
unsigned int FlamingoSwitch::nReceivedDelay = 0;
unsigned int FlamingoSwitch::timings[FLAMINGO_MAX_CHANGES];

FlamingoSwitch::FlamingoSwitch()
{
	this->nTransmitterPin = -1;
	this->nReceiverInterrupt = -1;
	FlamingoSwitch::nReceivedValue = NULL;
	this->nPulseLength = 300;
}


/**
* Enable transmissions
*
* @param nTransmitterPin    Arduino Pin to which the sender is connected to
*/
void FlamingoSwitch::enableTransmit(int nTransmitterPin)
{
	this->nTransmitterPin = nTransmitterPin;
	pinMode(this->nTransmitterPin, OUTPUT);
}

/**
* Disable transmissions
*/
void FlamingoSwitch::disableTransmit()
{
	this->nTransmitterPin = -1;
}

void FlamingoSwitch::transmit(int nHighPulses, int nLowPulses)
{
	boolean disabled_Receive = false;
	int nReceiverInterrupt_backup = nReceiverInterrupt;

	if (this->nTransmitterPin != -1) 
	{
		if (this->nReceiverInterrupt != -1) 
		{
			this->disableReceive();
			disabled_Receive = true;
		}

		digitalWrite(this->nTransmitterPin, HIGH);
		delayMicroseconds(this->nPulseLength * nHighPulses);
		digitalWrite(this->nTransmitterPin, LOW);
		delayMicroseconds(this->nPulseLength * nLowPulses);

		if (disabled_Receive)
		{
			this->enableReceive(nReceiverInterrupt_backup);
		}
	}
}


void FlamingoSwitch::send(uint32_t code, unsigned int retries)
{
	unsigned long DataBit;
	unsigned long DataMask = 0x80000000;

	for (int i = 0; i <= retries; i++)
	{
		sendSync();

		for (int i = 0; i < 28; i++)   // Flamingo command is only 28 bits */
		{
			DataBit = code & DataMask; // Get most left bit
			code = (code << 1);        // Shift left

			if (DataBit != DataMask)
			{
				send0();
			}
			else
			{
				send1();
			}
		}
	}

}

/**
* Sends a "0" Bit
*                     _
* Waveform Protocol: | |___
*/
void FlamingoSwitch::send0()
{
	this->transmit(1, 3);
}

/**
* Sends a "1" Bit
*                      ___
* Waveform Protocol : |   |_
*/
void FlamingoSwitch::send1()
{
	this->transmit(3, 1);
}

/**
* Sends a "Sync" Bit
*                     _
* Waveform Protocol: | |_______________
*/
void FlamingoSwitch::sendSync()
{
	this->transmit(1, 15);
}

// ------------------------------------------------------------------------------------------------
void FlamingoSwitch::enableReceive(int interrupt)
{
	this->nReceiverInterrupt = interrupt;
	this->enableReceive();
}

void FlamingoSwitch::enableReceive()
{
	if (this->nReceiverInterrupt != -1) 
	{
		FlamingoSwitch::nReceivedValue = NULL;
		FlamingoSwitch::nReceivedBitlength = NULL;
		attachInterrupt(this->nReceiverInterrupt, handleInterrupt, CHANGE);
	}
}

void FlamingoSwitch::disableReceive()
{
	detachInterrupt(this->nReceiverInterrupt);
	this->nReceiverInterrupt = -1;
}


bool FlamingoSwitch::available() const
{
	return FlamingoSwitch::nReceivedValue != NULL;
}

void FlamingoSwitch::resetAvailable() 
{
	FlamingoSwitch::nReceivedValue = NULL;
}

unsigned long FlamingoSwitch::getReceivedValue() const
{
	return FlamingoSwitch::nReceivedValue;
}

unsigned int FlamingoSwitch::getReceivedBitlength() const
{
	return FlamingoSwitch::nReceivedBitlength;
}

unsigned int FlamingoSwitch::getReceivedDelay() const
{
	return FlamingoSwitch::nReceivedDelay;
}

unsigned int* FlamingoSwitch::getReceivedRawdata() const
{
	return FlamingoSwitch::timings;
}


bool FlamingoSwitch::receiveProtocol(unsigned int changeCount)
{
	unsigned long code = 0;
	unsigned long delay = FlamingoSwitch::timings[0] / 15;
	unsigned long delayTolerance = delay * FlamingoSwitch::nReceiveTolerance * 0.01;

	for (int i = 1; i<changeCount; i = i + 2) 
	{
		if (
			// 0:  1 3
			FlamingoSwitch::timings[i] > delay - delayTolerance &&
			FlamingoSwitch::timings[i] < delay + delayTolerance &&
			FlamingoSwitch::timings[i + 1] > delay * 3 - delayTolerance &&
			FlamingoSwitch::timings[i + 1] < delay * 3 + delayTolerance
			)
		{
			code = code << 1;
		}
		else if (
			// 1:  3 1
			FlamingoSwitch::timings[i] > delay * 3 - delayTolerance &&
			FlamingoSwitch::timings[i] < delay * 3 + delayTolerance &&
			FlamingoSwitch::timings[i + 1] > delay - delayTolerance &&
			FlamingoSwitch::timings[i + 1] < delay + delayTolerance
			)
		{
			code += 1;
			code = code << 1;
		}
		else 
		{
			// Failed
			i = changeCount;
			code = 0;
		}
	}

	code = code >> 1;

	if (changeCount > 6)
	{    // ignore < 4bit values as there are no devices sending 4bit values => noise
		FlamingoSwitch::nReceivedValue = code;
		FlamingoSwitch::nReceivedBitlength = changeCount / 2;
		FlamingoSwitch::nReceivedDelay = delay;
	}

	if (code == 0)
	{
		return false;
	}
	else if (code != 0)
	{
		return true;
	}
}

#define LIMIT_28BIT 4000 // the sync is about 5000 (15* 300us9
#define LIMIT_24BIT 6000

#define TOLERANCE 200
void FlamingoSwitch::handleInterrupt() 
{
	static unsigned int duration;
	static unsigned int changeCount;
	static unsigned long lastTime;
	static unsigned int repeatCount;

	long time = micros();
	duration = time - lastTime;

	// The device sends:
	// 4 times a 28Bit code
	// 6 times an unknown code
	// 6 times a 24Bit code
	// we are only interested in the first one (28Bit), so we filter away.
	// This is done by taking only the code with the short sync of 
	if (duration > LIMIT_28BIT && duration < LIMIT_24BIT)
	{
		repeatCount++;
		changeCount--;
		if (repeatCount == 2)
		{
			if (receiveProtocol(changeCount) == false)
			{
			}

			repeatCount = 0;
		}

		changeCount = 0;
	}
	else if (duration > LIMIT_24BIT)
	{
		changeCount = 0;
	}

	if (changeCount >= FLAMINGO_MAX_CHANGES) 
	{
		changeCount = 0;
		repeatCount = 0;
	}
	
	FlamingoSwitch::timings[changeCount++] = duration;
	lastTime = time;
}