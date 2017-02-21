/* (c) windkh 2015
MySensors 2.0
Flamingo Switch Sensor (Repeater)
required hardware:
- 1x Arduino
- 1x Radio NRF24L01+
- (1x 433MHz Receiver) (optional)
- 1x 433Mhz Sender
- 1x DHT-22
connect PIN3 to receiver unit Data-PIN
(connect PIN4 to sender unit Data-PIN) (optional)
connect PIN5 to DHT-22

The sketch registeres a sensor 0 which reports all received codes. This one can be used to sniff for codes, 
or to send incomming codes directly into the air.
The sensors 1-4 are hardcoded switches. You have to enter your codes from the remote control in the table 
DEVICE_CODES first. Every button on the remote control sends 4 different keys. Though only one is required 
the sketch is able to send all 4 after each other. All states are written into the eeprom. On restart the sketch will 
restore the switch states based on the saved values.
*/

#define MY_DEBUG_VERBOSE
#define MY_SPECIAL_DEBUG

// Enable debug prints to serial monitor
#define MY_DEBUG 

// Enable and select radio type attached
#define MY_RADIO_NRF24

// Enabled repeater feature for this node
#define MY_REPEATER_FEATURE

#include "FlamingoSwitch.h"
#include <MySensors.h> 
#include <SPI.h>
#include <EEPROM.h>  

#include <DHT.h>  

#include "Timer.h"


//-----------------------------------------------------------------------------
// Timer
#define SENSOR_UPDATE_INTERVAL 60000

//-----------------------------------------------------------------------------
// DHT22
#define CHILD_ID_TEMP 11
#define CHILD_ID_HUM 12
#define HUMIDITY_SENSOR_DIGITAL_PIN 5

DHT dht;
float lastTemp;
float lastHum;
MyMessage msgHum(CHILD_ID_HUM, V_HUM);
MyMessage msgTemp(CHILD_ID_TEMP, V_TEMP);

//-----------------------------------------------------------------------------
// Timer
Timer timer;

//-----------------------------------------------------------------------------
FlamingoSwitch Switch;

const int TX_PIN  = 4;
const int IRQ_PIN = 1; // IRQ1 = PIN 3 on Arduino Uno

#define OFF 0
#define ON  1

#define DEVICES	 4				/* Amount of units supported per remote control A-D*/
#define CODES_PER_DEVICE 4		/* Amount of codes stored per A-D*/
int OnCounter = 0;
int OffCounter = 0;

// the remote control sends 4 different codes of each state.
struct Codes
{
	uint32_t Code[CODES_PER_DEVICE];
};

// every device contains 4 + 4 codes.
struct Device
{
	Codes Off;
	Codes On;
};

Device DEVICE_CODES[DEVICES] =
{
	{ //  0          1          2          3 
		{ 0x25C90CE, 0x24DC206, 0x277193A, 0x267C51A }, // (28Bit) A OFF
		{ 0x24E77D6, 0x27263DE, 0x25B25B6, 0x24613E2 }  // (28Bit) A ON 
	},
	{
		{ 0x275BADD, 0x27F3115, 0x2440F0D, 0x26933A9 }, // (28Bit) B OFF
		{ 0x2547C99, 0x26585B9, 0x259A085, 0x2717B51 }  // (28Bit) B ON 
	},
	{
		{ 0xE73720D, 0xE616D1D, 0xE4D49F5, 0xE4936D5 }, // (28Bit) C OFF
		{ 0xE62AF39, 0xE56BF91, 0xE4FBCD9, 0xE56EB71 }  // (28Bit) C ON 
	},
	{
		{ 0x65B67B6, 0x64C8BFA, 0x6504C32, 0x6526F4A }, // (28Bit) D OFF
		{ 0x670319A, 0x65E1942, 0x643F986, 0x65F1C2A }  // (28Bit) D ON 
	}
};

#define BAUDRATE 115200

void send(uint32_t code)
{
	Serial.print("Sending ");
	Serial.print(code, HEX);
	Serial.println("");

	Switch.send(code, 0);
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

	uint32_t sendBuffer;
	if (state == OFF)
	{
		int i = OffCounter++ % CODES_PER_DEVICE;
		sendBuffer = DEVICE_CODES[device].Off.Code[i];
	}
	else
	{
		int i = OnCounter++ % CODES_PER_DEVICE;
		sendBuffer = DEVICE_CODES[device].On.Code[i];
	}

	send(sendBuffer);
}

// timer handler to read and send temperature
void onTimer()
{
	readHumidity();
}

void readHumidity()
{
	float temperature = dht.getTemperature();
	if (isnan(temperature))
	{
		Serial.println("Failed reading temperature from DHT");
	}
	else
	//else if (temperature != lastTemp)
	{
		lastTemp = temperature;
		Serial.print("T: ");
		Serial.print(temperature);
		Serial.println(" C");
		send(msgTemp.set(lastTemp, 1));
	}

	float humidity = dht.getHumidity();
	if (isnan(humidity))
	{
		Serial.println("Failed reading humidity from DHT");
	}
	else
	//else if (humidity != lastHum)
	{
		lastHum = humidity;
		Serial.print("H: ");
		Serial.print(humidity);
		Serial.println(" \%");
		send(msgHum.set(lastHum, 1));
	}
}


// setup serial communication, initialize the pins for communication between arduino and rx and tx units.
// Announce sensor 0 and sensor 1..n
void presentation() 
{
	Serial.begin(BAUDRATE);
	sendSketchInfo("Flamingo Switch", "2.0");

	// setup 433Mhz
	Switch.enableReceive(IRQ_PIN);
	Switch.enableTransmit(TX_PIN);

	// DHT Sensor
	dht.setup(HUMIDITY_SENSOR_DIGITAL_PIN);
	present(CHILD_ID_TEMP, S_TEMP);
	present(CHILD_ID_HUM, S_HUM);

	// id 0 is a general rx tx sensor, it announces the received code and sends the one comming from the controller.
	{
		present(0, S_CUSTOM);
		MyMessage message(0, V_VAR1);
		message.set(0);
		send(message);
	}

	// id 0..n are predefined switches with hardcoded codes.
	for (uint8_t i = 0; i < DEVICES; i++)
	{
		uint8_t sensorId = i + 1; // sensor 0 is a generic send / receive device
		present(sensorId, S_LIGHT);
		bool state = loadState(i);
		send(i, state); 

		MyMessage message(sensorId, V_LIGHT);
		message.set(state);
		send(message);
	}

	// Start timer to read hum and temp.
	//int tickEvent = 
	timer.every(SENSOR_UPDATE_INTERVAL, onTimer);
}

// processes my sensors data. Receives codes and sends them to the mysensors controller.
void loop()
{
	if (Switch.available())
	{
		unsigned long code = Switch.getReceivedValue();
		
		Serial.print("Detected code:");
		Serial.print(code, HEX);
		Serial.println("");

		MyMessage message(0, V_VAR1);
		message.set(code);
		send(message);

		Switch.resetAvailable();
	}

	timer.update();
}

// imcomming message handler.
// 0: sensor directly transmits the values received from the mysensors controller.
// 1..n: the switch is turned on/off with the predefined codes. 
void receive(const MyMessage& message)
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

		send(state);

		MyMessage message(sensor, V_VAR1);
		message.set(state);
		send(message);
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
			saveState(sensor, state);

			MyMessage message(sensor, V_LIGHT);
			message.set(state);
			send(message);
		}
	}
}
