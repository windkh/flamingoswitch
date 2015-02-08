/* (c) windkh 2015
Flamingo Switch Sensor for see www.mysensors.org
required hardware:
- 1x Arduino
- 1x Radio NRF24L01+
- 1x 433MHz Receiver
- 1x 433Mhz Sender
connect PIN3 to receiver unit Data-PIN
connect PIN4 to sender unit Data-PIN

The sketch registeres a sensor 0 which reports all received codes. This one can be used to sniff for codes, 
or to send incomming codes directly into the air.
The sensors 1-4 are hardcoded switches. You have to enter your codes from the remote control in the table 
DEVICE_CODES first. Every button on the remote control sends 4 different keys. Though only one is required 
the sketch is able to send all 4 after each other. All states are written into the eeprom. On restart thesketch will 
restore the switch states based on the saved values.
*/

#include "FlamingoSwitch.h"
#include <MySensor.h> 
#include <SPI.h>
#include <EEPROM.h>  

//-----------------------------------------------------------------------------
// MySensor
#define CE_PIN			 9  // orange 
#define CS_PIN           10 // yellow
MySensor gw(CE_PIN, CS_PIN);

//-----------------------------------------------------------------------------
FlamingoSwitch Switch;

const int TX_PIN  = 4;
const int IRQ_PIN = 1; // PIN 3;

#define OFF 0
#define ON  1

#define DEVICES	 4  /* Amount of units supported per remote control A-D*/

// the remote control sends 4 different codes of each state.
struct Codes
{
	uint32_t Code1;
	uint32_t Code2;
	uint32_t Code3;
	uint32_t Code4;
};

// every device contains 4 + 4 codes.
struct Device
{
	Codes Off;
	Codes On;
};

Device DEVICE_CODES[DEVICES] =
{
	{
		{ 0x24DC2060, 0x267C51A0, 0x25C90CE0, 0x277193A0 }, // (28Bit) A OFF
		{ 0x25B25B60, 0x24613E20, 0x27263DE0, 0x24E77D60 }  // (28Bit) A ON 
	},
	{
		{ 0x275BADD0, 0x26933A90, 0x27F31150, 0x2440F0D0 }, // (28Bit) B OFF
		{ 0x2717B510, 0x2547C990, 0x26585B90, 0x259A0850 }  // (28Bit) B ON 
	},
	{
		{ 0xE4936D50, 0xE73720D0, 0xE616D1D0, 0xE4D49F50 }, // (28Bit) C OFF
		{ 0xE56BF910, 0xE56EB710, 0xE62AF390, 0xE4FBCD90 }  // (28Bit) C ON 
	},
	{
		{ 0x65B67B60, 0x64C8BFA0, 0x6504C320, 0x6526F4A0 }, // (28Bit) D OFF
		{ 0x65F1C2A0, 0x670319A0, 0x65E19420, 0x643F9860 }  // (28Bit) D ON 
	}
};

const int BAUDRATE = BAUDRATE;
const int RETRIES = 1; 

void send(uint32_t code)
{
	Serial.print("Sending ");
	Serial.print(code, HEX);
	Serial.println("");

	Switch.send(code, RETRIES);
}

// send the state (on/off) to the switch 1..n
void send(uint8_t device, uint8_t state)
{
	// check if device code between 0-3 (A-D)
	if (device >= DEVICES)
	{
		Serial.print("Device error: ");
		Serial.println(device);
		return;
	}

	// check if state = 0 (off) or 1 (on)
	if (state > 1)
	{
		Serial.print("Command error: ");
		Serial.println(state);
		return;
	}

	Serial.println("Send Flamingo command ");
	Serial.print("device=");
	Serial.println(device);
	Serial.print("state=");
	Serial.print(state);
	Serial.println();

	if (state == OFF)
	{
		send(DEVICE_CODES[device].Off.Code1);		
		send(DEVICE_CODES[device].Off.Code2);
		send(DEVICE_CODES[device].Off.Code3);
		send(DEVICE_CODES[device].Off.Code4);
	}
	else
	{
		send(DEVICE_CODES[device].On.Code1);
		send(DEVICE_CODES[device].On.Code2);
		send(DEVICE_CODES[device].On.Code3);
		send(DEVICE_CODES[device].On.Code4);
	}
}

// setup serial communication, initialize the pins for communication between arduino and rx and tx units.
// Announce sensor 0 and sensor 1..n
void setup()
{
	Serial.begin(BAUDRATE);

	// setup 433Mhz
	Switch.enableReceive(IRQ_PIN);
	Switch.enableTransmit(TX_PIN);

	// setup sensor
	gw.begin(incomingMessage, AUTO, true);
	gw.sendSketchInfo("Flamingo Switch", "1.1");

	// id 0 is a general rx tx sensor, it announces the received code and sends the one comming from the controller.
	{
		gw.present(0, S_CUSTOM);
		MyMessage message(0, V_VAR1);
		message.set(0);
		gw.send(message);
	}

	// id 1..n are predefined switches with hardcoded codes.
	for (uint8_t i = 0; i < DEVICES; i++)
	{
		uint8_t sensorId = i + 1; // sensor 0 is a generic send / receive device
		gw.present(sensorId, S_LIGHT);
		bool state = gw.loadState(i);
		send(sensorId, state);

		MyMessage message(sensorId, V_LIGHT);
		message.set(state);
		gw.send(message);
	}
}

// processes my sensors data. Receives codes and sends them to the mysensors controller.
void loop()
{
	gw.process();

	if (Switch.available())
	{
		unsigned long code = Switch.getReceivedValue();
		
		uint32_t state = code << 4; // 28Bit --> 32Bit

		Serial.print("Detected code:");
		Serial.print(state, HEX);
		Serial.println("");

		MyMessage message(0, V_VAR1);
		message.set(code);
		gw.send(message);

		Switch.resetAvailable();
	}
}

// imcomming message handler.
// 0: sensor directly transmits the values received from the mysensors controller.
// 1..n: the switch is turned on/off with the predefined codes. 
void incomingMessage(const MyMessage& message)
{
	if (message.isAck()) 
	{
		Serial.println("This is an ack from gateway");
	}

	uint8_t sensor = message.sensor;
	if (sensor == 0)
	{
		unsigned long state = message.getULong();
			
		Serial.print("Incoming code: ");
		Serial.print(state, HEX);
		Serial.println("");

		Switch.send(state);

		MyMessage message(sensor, V_VAR1);
		message.set(state);
		gw.send(message);
	}
	else
	{
		uint8_t sensor = message.sensor;
		Serial.print("Incoming change for switch:");
		Serial.print(sensor);
		Serial.print(", value: ");
		Serial.print(message.getBool(), DEC);
		Serial.println("");

		if (message.type == V_LIGHT)
		{
			bool state = message.getBool();
			send(sensor-1, state); // -1 as we start counting the sensors at 1 instead of 0
			gw.saveState(sensor, state);

			MyMessage message(sensor, V_LIGHT);
			message.set(state);
			gw.send(message);
		}
	}
}