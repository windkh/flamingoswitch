/* 
Created by Karl-Heinz Wind - karl-heinz.wind@web.de
Copyright 2015 License: GNU GPL v3 http://www.gnu.org/licenses/gpl-3.0.html

Flamingo switch sample.

required hardware:
- 1x Arduino
- 1x 433MHz Receiver
- 1x 433Mhz Sender
connect PIN3 to receiver-unit Data-PIN
connect PIN4 to sender-unit Data-PIN

The sketch listenes for codes from the remote control and prints em via serial.
Every button sends 4 different codes, so don't be confused when you read different codes
when pressing the same button several times. indeed you need only one of the four to control
the socket.
*/

#include "FlamingoSwitch.h"

FlamingoSwitch Switch;

#define DEVICES  4  /* Number of sockets supported */
#define COMMAND  2  /* 0 = On, 1 = Off */


int RX_PIN = 1; // PIN3 rx needs to be in IRQ pin!
int TX_PIN = 4; // PIN4

#define ON  0
#define OFF 1

uint32_t DEVICE_CODES[DEVICES][COMMAND] = 
{
	0x25B25B60,// (28Bit) Binary: 0010010110110010010110110110 A ON
	0x24DC2060,// (28Bit) Binary: 0010010011011100001000000110 A OFF

	0x2717B510,// (28Bit) Binary: 0010011100010111101101010001 B ON
	0x275BADD0,// (28Bit) Binary: 0010011101011011101011011101 B OFF

	0xE56BF910,// (28Bit) Binary: 1110010101101011111110010001 C ON
	0xE4D49F50,// (28Bit) Binary: 1110010011010100100111110101 C OFF

	0x65F1C2A0,// (28Bit) Binary: 0110010111110001110000101010 D ON
	0x65B67B60,// (28Bit) Binary: 0110010110110110011110110110 D OFF
};

                  
void send(int device, int command)
{
	Serial.print("Sending device: ");
	Serial.print(device);
	Serial.print(" command: ");
	Serial.println(command);

	uint32_t sendBuffer = DEVICE_CODES[device][command];
	Switch.send(sendBuffer);
}

void setup() 
{
	Serial.begin(115200);

	Switch.enableReceive(RX_PIN);  // Receiver on interrupt 0 => that is pin #2
	Switch.enableTransmit(TX_PIN);
}

void loop() 
{
	// detect code from remote control
	if (Switch.available())
	{
		output(Switch.getReceivedValue(), Switch.getReceivedBitlength(), Switch.getReceivedDelay(), Switch.getReceivedRawdata());
		Switch.resetAvailable();
	}

	// Example for sending the codes
	// send(0, ON);
	// send(0, OFF);
}

void output(unsigned long code, unsigned int length, unsigned int delay, unsigned int* raw) 
{
	if (code != 0)
	{
		char* b = code2bin(code, length);
		Serial.print("Code: 0x");
		Serial.print(code, HEX);
		Serial.print("0 ("); // as the code is 28Bit we add a zero to get a 32Bit number which can be copied into our sketch
		Serial.print(length);
		Serial.print("Bit) Binary: ");
		Serial.print(b);
		Serial.print(" PulseLength: ");
		Serial.print(delay);
		Serial.print(" microseconds");
		Serial.println("");
	}
	else
	{
		Serial.print("Unknown encoding.");
	}

	Serial.print("Raw data: ");
	for (int i = 0; i <= length * 2; i++)
	{
		Serial.print(raw[i]);
		Serial.print(",");
	}

	Serial.println();
	Serial.println();
}

static char * code2bin(unsigned long code, unsigned int bitLength)
{
	static char bin[64];
	unsigned int i = 0;

	while (code > 0)
	{
		bin[32 + i++] = (code & 1 > 0) ? '1' : '0';
		code = code >> 1;
	}

	for (unsigned int j = 0; j< bitLength; j++)
	{
		if (j >= bitLength - i)
		{
			bin[j] = bin[31 + i - (j - (bitLength - i))];
		}
		else
		{
			bin[j] = '0';
		}
	}
	bin[bitLength] = '\0';

	return bin;
}
