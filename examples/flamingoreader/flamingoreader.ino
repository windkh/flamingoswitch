/*
 Thanks to wex_storm for contributing this on
 http://forum.arduino.cc/index.php?topic=201771.15

 Flamingo switch sample.

 required hardware:
 - 1x Arduino
 - 1x 433MHz Receiver

 connect PIN2 to receiver-unit Data-PIN

 The sketch listenes for codes from the remote control and prints em via serial.
 Every button sends 4 different codes, so don't be confused when you read different codes
 when pressing the same button several times. indeed you need only one of the four to control
 the socket. The arduino needs to be reset after all 4 codes of one button were detected to
 listen to the next one!
 */

#include "FlamingoSwitch.h"

FlamingoSwitch Switch;

// Change this number if your remote control supplies more than 4 codes.
const int CODE_COUNT = 4;

// Receive PIN needs to be an IRQ pin!
int RX_PIN = 0; // <-- IRQ 0 is PIN2 on a Arduino Uno


int counter = 0;
int deviceCounter = 0;

uint32_t codes[CODE_COUNT] = {
	0, 0, 0, 0 };

String bins[CODE_COUNT] = {
	"", "", "", "" };

void setup()
{
	Serial.begin(9600);

	Switch.enableReceive(RX_PIN);  // Receiver on interrupt 0 => that is pin #2
}

void loop()
{
	// detect code from remote control
	if (Switch.available())
	{
		output(Switch.getReceivedValue(), Switch.getReceivedBitlength(), Switch.getReceivedDelay(), Switch.getReceivedRawdata());
		Switch.resetAvailable();
	}
}

void output(unsigned long code, unsigned int length, unsigned int delay, unsigned int* raw)
{
	counter++;
	if (code != 0)
	{
		char* b = code2bin(code, length);
		String bin(b);

		if (deviceCounter == 0)
		{
			Serial.println("Scan started.");
			codes[0] = code;
			bins[0] = bin;
			deviceCounter++;
		}

		boolean found = false;
		for (int i = 0; i < deviceCounter; i++)
		{
			if (code == codes[i])
			{
				found = true;
			}
		}

		if (!found)
		{
			codes[deviceCounter] = code;
			bins[deviceCounter] = bin;
			deviceCounter++;
		}

		Serial.println();
		Serial.print(" Bin: ");
		Serial.println(b);
		Serial.println();
		for (int i = 0; i < deviceCounter; i++)
		{
			uint32_t codeFromList = codes[i];
			Serial.print("Counter: ");
			Serial.print(i);
			Serial.print(" Code: ");
			Serial.print(codeFromList);
			Serial.print(" Code: 0x");
			Serial.print(codeFromList, HEX);
			Serial.print(" Bin: ");
			Serial.print(bins[i]);

			uint16_t receiverId;
			uint8_t value;
			uint8_t rollingCode;
			uint16_t transmitterId;
			Switch.decrypt(codeFromList, receiverId, value, rollingCode, transmitterId);

			Serial.print(" Button: ");
			Serial.print(receiverId, DEC);
			Serial.print(" ON/OFF/DIM: ");
			Serial.print(value, DEC);
			Serial.print(" Rolling-Code: ");
			Serial.print(rollingCode, DEC);
			Serial.print(" TransmitterId: 0x");
			Serial.print(transmitterId, HEX);

			Serial.println();
		}

		if (deviceCounter == CODE_COUNT)
		{
			Serial.println("Scan complete.");
			deviceCounter = 0;
			for (int i = 0; i < deviceCounter; i++)
			{
				bins[i] = "";
				codes[i] = 0;
			}
		}
	}
	else
	{
		Serial.print("Unknown encoding.");
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

	for (unsigned int j = 0; j < bitLength; j++)
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


