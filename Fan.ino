
/**
 * LED Board controller
 * Tim Peters
 * FaimMedia B.V.
**/

#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include <DallasTemperature.h>

/**
 * Temperature pins
 */
#define PIN_TEMPERATURE_1 8
#define PIN_TEMPERATURE_2 9

/**
 * Relay pins
 */
const int PIN_RELAY[4] = {
	48,
	49,
	50,
	51,
};

/**
 * Fan PMW pins (write speed)
 * (must be analog pins)
 */
const int PIN_FAN_PWM[6] = {
	7,
	6,
	5,
	4,
	3,
	2,
};

/**
 * Fan sensor pins (read speed)
 */
const int PIN_FAN_SENSOR[6] = {
	22,
	24,
	26,
	28,
	30,
	32,
};

/**
 * Buttons
 */
const int PIN_BUTTON[3] = {
	10,  // Green
	11,  // Yellow
	12,  // Red
};

/**
 * Fan groups
 */
const int FAN_GROUP[4][2] = {
	{0,  1},
	{2, -1},
	{3, -1},
	{4,  5},
};

/**
 * Timeout between occurance
 */
const int MS_BUTTON = 50;

/**
 * Variables
 */

unsigned long lastTemperatureTime = 0;
unsigned long lastButtonTime = 0;

float lastTemperature[2] = {
	0,
	0,
};

float sumTemperature[2] = {
	0,
	0,
};

float maxTemperature[2] = {
	0,
	0,
};

int sequenceTemperature[2] = {
	0,
	0,	
};

int lastButtonValue[3] = {
	HIGH,
	HIGH,
	HIGH,
};

unsigned long lastButtonMs[3] = {
	0,
	0,
	0,
};

int fanSpeed[6] = {
	false,
	false,
	false,
	false,
	false,
	false,
};

unsigned long fanSpeedRpm[6] = {
	0, 0, 0, 0, 0, 0,
};

unsigned long int fanSpeedRpmTime[6] = {
	0, 0, 0, 0, 0, 0,
};

bool tempActive[2] = {
	true,
	true,
};

/**
 * The time in micro seconds the fan speed should update
 **/
const int MS_FAN_SPEED_RPM = 2000;

/**
 * The time in micro seconds the display should update
 **/
const int MS_DISPLAY_CHANGE = 500;

/**
 * The time in micro seconds the fans should update on temperature changes
 **/
const int MS_FAN_CHANGE = 1000;


const int MENU_ITEM_MAIN = 0;
const int MENU_ITEM_RPM = 1;
const int MENU_ITEM_TEMP = 2;

int menuItem = 0;
long menuChange = 0;

unsigned long nextDisplayChange = 0;
unsigned long nextFanChange = 0;

/**
 * Init Libraries
 **/
Adafruit_SSD1306 display(128, 32, &Wire, -1);

OneWire oneWire[2] = {
	OneWire(PIN_TEMPERATURE_1),
	OneWire(PIN_TEMPERATURE_2),
};

DallasTemperature sensor[2] = {
	DallasTemperature(&oneWire[0]),
	DallasTemperature(&oneWire[1]),
};

/**
 * METHODS
 **/

/**
 * Number format
 **/
String numberFormat(float value, int length, int decimal)
{
	char valueOut[length];
	dtostrf(value, length - 1, 1, valueOut);

	return String(valueOut);
}

/**
 * Simple zero fill (strpad) method
 */
String zeroFill(int number, unsigned int padCount = 3, String padChar = "0")
{
	String string = String(number);

	while(string.length() < padCount) {
		string = padChar + string;
	}

	return string;
}

/**
 * Show changed menu item
 **/
void setMenuItem()
{
	display.clearDisplay();
	display.setCursor(0, 10);
	display.setTextSize(2);

	if (menuItem == MENU_ITEM_RPM) {
		display.print("RPM Menu");
	}

	if (menuItem == MENU_ITEM_TEMP) {
		display.print("Temp");
	}

	if (menuItem == MENU_ITEM_MAIN) {
		display.print("Main menu");
	}

	display.display();
}

/**
 * Create display
 **/
void createDisplay()
{
	display.clearDisplay();
	display.setCursor(0, 0);

	if (menuItem == MENU_ITEM_TEMP) {
		for (int i = 0; i < 2; i++) {
			float temp = getTemperature(i);

			display.setCursor(0, i * 16);
			display.setTextSize(2);

			display.print("T");
			display.print(i + 1);
			display.print(": ");
			display.print(numberFormat(temp, 5, 1));
			display.print("");
			display.print((char)247);
			display.print("C");
		}
	}

	if (menuItem == MENU_ITEM_MAIN) {
		display.setTextSize(1);

		// temp sensor 1
		for (int i = 0; i < 2; i++) {
			display.setCursor(i * 64, 0);

			float temp = getTemperature(i);

			display.print("T");
			display.print(i + 1);
			display.print(": ");
			display.print(numberFormat(temp, 5, 1));
			display.print("");
			display.print((char)247);
			display.print("C");
		}

		// set fan group percentage
		int y = 16;
		int fanIndexes[4] = {0, 2, 3, 4};
		for (int i = 0; i < 4; i++)
		{
			int fanIndex = fanIndexes[i];

			display.setCursor((i % 2) * 64, y);

			display.print("F");
			display.print(fanIndex + 1);
			display.print(": ");
			display.print(zeroFill(fanSpeed[fanIndex], 3, " "));
			display.print(" %");

			if (i % 2) {
				y += 8;
			}
		}
	}

	if (menuItem == MENU_ITEM_RPM) {
		display.setTextSize(1);

		int y = 0;
		for (int i = 0; i < 6; i++) {
			display.setCursor((i % 2) * 64, y);

			long int speed = getFanSpeed(i);

			display.print("");
			display.print(i + 1);
			display.print(":");
			display.print(zeroFill(speed, 4, " "));
			display.print("RPM");

			if (i % 2) {
				y += 11;
			}
		}
	}

	display.display();

	nextDisplayChange = millis() + MS_DISPLAY_CHANGE;
}

/**
 * Initialize relais
 */
void initRelais()
{
	for(int i = 0; i < 4; i++) {
		pinMode(PIN_RELAY[i], OUTPUT);
		digitalWrite(PIN_RELAY[i], HIGH);
	}
}

/**
 * Initialize buttons
 */
void initButtons()
{
	pinMode(PIN_BUTTON[0], INPUT_PULLUP);
	pinMode(PIN_BUTTON[1], INPUT_PULLUP);
	pinMode(PIN_BUTTON[2], INPUT_PULLUP);
}

/**
 * Initialize fans
 */
void initFans()
{
	for(int i = 0; i < 6; i++) {
		pinMode(PIN_FAN_PWM[i], OUTPUT);
		pinMode(PIN_FAN_SENSOR[i], INPUT);

		digitalWrite(PIN_FAN_SENSOR[i], HIGH);
	}
}

/**
 * Get temperature
 **/
float getTemperature(int index)
{
	if (millis() > lastTemperatureTime || !lastTemperature[index] ) {
		sensor[index].requestTemperatures();

		float temp = sensor[index].getTempCByIndex(0);

		if (temp > 0) {
			lastTemperature[index] = temp;
		}
	}

	return lastTemperature[index];
}

/**
 * Set loading screen
 */
void setLoading()
{
	int y = 24;
	int h = 6;

	// set bar
	display.drawRect(2, y, display.width()-2, h, SSD1306_WHITE);
	display.display();

	for(int i = 3; i < display.width()-3; i++) {

		display.fillRect(i, y, i + 1, h, SSD1306_WHITE);
		display.display();

		if(20 - i < 10) {
			delay(1);
		} else {
			delay(20 - i);
		}
	}
}

/**
 * Check if button value has changed
 */
bool checkButtonValue(int index)
{
	int buttonValue = digitalRead(PIN_BUTTON[index]);

	if (buttonValue != lastButtonValue[index] && millis() > lastButtonMs[index] + MS_BUTTON) {
		lastButtonValue[index] = buttonValue;
		lastButtonMs[index] = millis();

		digitalWrite(LED_BUILTIN, LOW);

		if(buttonValue == LOW) {
			digitalWrite(LED_BUILTIN, HIGH);

			return true;
		}
	}

	return false;
}

/**
 * Set fan group speed
 */
void setFanGroupSpeed(int index, int speed)
{
	// set relay
	int relayValue = HIGH;
	if(speed > 0) {
		relayValue = LOW;
	}

	if (speed > 100) {
		speed = 100;
	}

	double increment = 2.55;
	int rSpeed = speed * increment;

	digitalWrite(PIN_RELAY[index], relayValue);

	for (int i = 0; i < 2; i++) {
		int fanIndex = FAN_GROUP[index][i];

		if (fanIndex == -1) {
			continue;
		}

		fanSpeed[fanIndex] = speed;

		analogWrite(PIN_FAN_PWM[fanIndex], rSpeed);
	}
}

/**
 * Get fan speed in RPM
 */
long int getFanSpeed(int index)
{
	if (millis() < fanSpeedRpmTime[index]) {
		return fanSpeedRpm[index];
	}

	fanSpeedRpmTime[index] = millis() + MS_FAN_SPEED_RPM;

	if (!fanSpeed[index]) {
		fanSpeedRpm[index] = 0;

		return 0;
	}

	long int rpm = 0;
	int pin = PIN_FAN_SENSOR[index];

	//unsigned long pulseDuration = analogRead(pin);

	long int pulseDuration = pulseIn(pin, LOW);
	double frequency = 1000000 / pulseDuration;

	rpm = frequency / 4 * 60;

	fanSpeedRpm[index] = rpm;

	return rpm;
}

/**
 * Test which fan is connected to which relay
 **/
void testFanRelay()
{
	int prevRelay = -1;
	int prev = -1;
	for (int i = 0; i < 6; i++)
	{
		if (prev > -1) {
			analogWrite(PIN_FAN_PWM[prev], 0);
		}

		analogWrite(PIN_FAN_PWM[i], 255);

		prev = i;

		for (int z = 0; z < 4; z++) {
			if (prevRelay >= 0) {
				digitalWrite(PIN_RELAY[prevRelay], HIGH);
			}

			digitalWrite(PIN_RELAY[z], LOW);

			prevRelay = z;

			display.clearDisplay();
			display.setTextSize(2);
			display.setCursor(0, 0);
			display.print("Fan ");
			display.print(i + 1);

			display.setCursor(0, 16);
			display.print("Relay ");
			display.print(z + 1);

			display.display();

			delay(5000);
		}
	}

	while(1);
}

/**
 * Get average temperature
 **/
float getAvgTemperature (int index) {

	float temp = getTemperature(index);

	if (temp > maxTemperature[index]) {
		maxTemperature[index] = temp;
		sumTemperature[index] = 0;
		sequenceTemperature[index] = 0;

		return temp;
	}

	sumTemperature[index] += temp;
	sequenceTemperature[index]++;

	if (sequenceTemperature[index] >= 30) {
		float avg = sumTemperature[index] / sequenceTemperature[index];
		maxTemperature[index] = avg;

		// reset avg
		sumTemperature[index] = 0;
		sequenceTemperature[index] = 0;

		return avg;		
	}
	
	return maxTemperature[index];
}

/**
 * APPLICATION
 **/

/**
 * Setup method
 **/
void setup()
{
	/**
	 * Set relais
	 **/
	initRelais();

	/**
	 * Set buttons
	 */
	initButtons();

	/**
	 * Set fans
	 */
	initFans();

	/**
	 * Set internal LED
	 */
	pinMode(LED_BUILTIN, OUTPUT);
	digitalWrite(LED_BUILTIN, LOW);

	if(!display.begin(SSD1306_SWITCHCAPVCC, 0x3C)) {
		Serial.println(F("SSD1306 allocation failed"));
		for(;;); // Don't proceed, loop forever
	}

	/**
	 * Set display
	 */
	display.clearDisplay();

	display.setTextSize(1);
	display.setTextColor(SSD1306_WHITE);

	display.setCursor(0, 0);
	display.print((char)26);
	display.print(" Created with ");
	display.println((char)3);

	display.setCursor(0, 10);
	display.println("  by FaimMedia");
	display.display();

	setLoading();
}

/**
 * Loop
 */
void loop()
{
	if(checkButtonValue(0)) {
		menuItem++;
		if (menuItem >= 3) {
			menuItem = 0;
		}

		setMenuItem();
		nextDisplayChange = millis() + 800;
	}

	if(checkButtonValue(1)) {
	}

	if(checkButtonValue(2)) {
		menuItem = 0;
		setMenuItem();
		nextDisplayChange = millis() + 800;
	}

	int min;
	int max;

	// check fan changes
	if (millis() > nextFanChange) {

		// temp left
		min = 25;
		max = 50;

		float temp = getAvgTemperature(0);
		if (temp > min) {
			float calcSpeed = temp - min;
			calcSpeed = calcSpeed / (max - min) * 100;

			setFanGroupSpeed(0, calcSpeed);
			setFanGroupSpeed(1, calcSpeed + 20);
		} else {
			setFanGroupSpeed(0, 0);
			setFanGroupSpeed(1, 0);
		}

		// temp right
		min = 25;
		max = 40;

		temp = getAvgTemperature(1);
		if (temp > min) {
			float calcSpeed = temp - min;
			calcSpeed = calcSpeed / (max - min) * 100;

			setFanGroupSpeed(3, calcSpeed);
		} else {
			setFanGroupSpeed(3, 0);
		}

		nextFanChange = millis() + MS_FAN_CHANGE;
	}

	if (millis() > nextDisplayChange) {
		createDisplay();
	}
}
