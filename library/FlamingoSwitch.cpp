// ---------------------------------------------------------------------------
// Flamingo Switch Library - v2.0
// ---------------------------------------------------------------------------


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
    this->nPulseLength = 330;
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

    for (int j = 0; j <= retries; j++)
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

    for (int i = 1; i < changeCount; i = i + 2)
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

#define LIMIT_28BIT 4000 // the sync is about 4875 (15* 325us)
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
    // 6 times a 32Bit code
    // 6 times a 24Bit code
    // we are only interested in the first one (28Bit), so we filter the others away.
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


/*
	Encode and decode routines are based on
	http ://cpp.sh/9ye4
	https ://github.com/r10r/he853-remote
	Thanks to fuchks from the FHEM forum for providing this
	see also https ://forum.fhem.de/index.php/topic,36399.60.html
*/

static uint8_t ikey[16] = { 5, 12, 6, 2, 8, 11, 1, 10, 3, 0, 4, 14, 7, 15, 9, 13 };  //invers cryptokey (exchanged index & value)
/*
    Extracts from the input (28-Bit code needs to be aligned to the right: 0x0.......):
    button = receiverId
    value (0=OFF, 1=ON, DIM)
    rolling code 0..n
    transmitterId of the remote control.
    */
void FlamingoSwitch::decrypt(uint32_t input, uint16_t& receiverId, uint8_t& value, uint8_t& rollingCode, uint16_t& transmitterId)
{
    uint8_t mn[7];	// message separated in nibbles

    input = ((input << 2) & 0x0FFFFFFF) | ((input & 0xC000000) >> 0x1a);		//shift 2 bits left & copy bit 27/28 to bit 1/2
    mn[0] = input & 0x0000000F;
    mn[1] = (input & 0x000000F0) >> 0x4;
    mn[2] = (input & 0x00000F00) >> 0x8;
    mn[3] = (input & 0x0000F000) >> 0xc;
    mn[4] = (input & 0x000F0000) >> 0x10;
    mn[5] = (input & 0x00F00000) >> 0x14;
    mn[6] = (input & 0x0F000000) >> 0x18;

    mn[6] = mn[6] ^ 9;										// no decryption

    //XOR decryption 2 rounds
    for (uint8_t r = 0; r <= 1; r++)
    {														// 2 decryption rounds
        for (uint8_t i = 5; i >= 1; i--)
        {													// decrypt 4 nibbles
            mn[i] = ((ikey[mn[i]] - r) & 0x0F) ^ mn[i - 1];	// decrypted with predecessor & key
        }
        mn[0] = (ikey[mn[0]] - r) & 0x0F;					//decrypt first nibble
    }

    //Output decrypted message 
    //uint32_t in = (~((input >> 2) | (((input & 3) << 0x1a))) << 4);
    receiverId = (uint16_t)mn[0];
    value = (((mn[1] >> 1) & 1) + (mn[6] & 0x7) + ((mn[6] & 0x8) >> 3));
    rollingCode = (mn[1] >> 2);
    transmitterId = (mn[5] << 12) + (mn[4] << 8) + (mn[3] << 4) + (mn[2] << 0);
}


static uint8_t key[17] = { 9, 6, 3, 8, 10, 0, 2, 12, 4, 14, 7, 5, 1, 15, 11, 13, 9 }; //cryptokey 

/*
    Encrypts the
    button = receiverId
    value (0=OFF, 1=ON; DIM)
    rolling code 0..n
    transmitterId of the remote control.
	28-Bit code is aligned to the right (0x0.......)!
    */
uint32_t FlamingoSwitch::encrypt(uint8_t receiverId, uint8_t value, uint8_t rollingCode, uint16_t transmitterId)
{
    uint8_t mn[7];
    
    mn[0] = receiverId;								// mn[0] = iiiib i=receiver-ID
    mn[1] = (rollingCode << 2) & 15; 				// 2 lowest bits of rolling-code
    if (value > 0)
    {												// ON or OFF
        mn[1] |= 2;
    }												// mn[1] = rrs0b r=rolling-code, s=ON/OFF, 0=const 0?
    mn[2] = transmitterId & 15;						// mn[2..5] = ttttb t=transmitterId in nibbles -> 4x ttttb
    mn[3] = (transmitterId >> 4) & 15;
    mn[4] = (transmitterId >> 8) & 15;
    mn[5] = (transmitterId >> 12) & 15;
    if (value >= 2 && value <= 9)
    {												// mn[6] = dpppb d = dim ON/OFF, p=%dim/10 - 1
        mn[6] = value - 2;							// dim: 0=10%..7=80%
        mn[6] |= 8;									// dim: ON
    }
    else
    {
        mn[6] = 0;									// dim: OFF
    }

    //XOR encryption 2 rounds
    for (uint8_t r = 0; r <= 1; r++)
    {												// 2 encryption rounds
        mn[0] = key[mn[0] - r + 1];					// encrypt first nibble
        for (uint8_t i = 1; i <= 5; i++)
        {											// encrypt 4 nibbles
            mn[i] = key[(mn[i] ^ mn[i - 1]) - r + 1];// crypted with predecessor & key
        }
    }

    mn[6] = mn[6] ^ 9;								// no  encryption

	uint32_t msg = 0;								// copy the encrypted nibbles in output buffer
	msg |= (uint32_t)mn[6] << 0x18;
	msg |= (uint32_t)mn[5] << 0x14;
	msg |= (uint32_t)mn[4] << 0x10;
	msg |= (uint32_t)mn[3] << 0x0c;
	msg |= (uint32_t)mn[2] << 0x08;
	msg |= (uint32_t)mn[1] << 0x04;
	msg |= (uint32_t)mn[0];
    msg = (msg >> 2) | ((msg & 3) << 0x1a);			// shift 2 bits right & copy lowest 2 bits of cbuf[0] in msg bit 27/28

    return msg;
}
