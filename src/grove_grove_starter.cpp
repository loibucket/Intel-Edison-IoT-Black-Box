// Loi Cheng 2015

// IoT Black Box

// Hardware Weekend Brooklyn 2015

// Build and run in Eclipse IDE with Intel IoT libraries

// https://software.intel.com/en-us/articles/install-eclipse-ide-on-intel-iot-platforms

#include "ttp223.h"
#include "grove.h"
#include "mic.h"
#include "jhd1313m1.h"
#include <climits>
#include <iostream>
#include <sstream>
#include <unistd.h>
#include "mraa.hpp"
#include "UdpClient.hpp"
#include <time.h>

//may need to manually link these libraries in Eclipse linker settings
#include "buzzer.h"
#include "groveloudness.h"
#include "servo.h"

/*
 * Preliminary Step
 *
 * Follow the IoT Cloud Analytics Getting Started Guide:
 * http://www.intel.com/support/motherboards/desktop/sb/CS-035346.htm
 *
 * Please check if the iotkit-agent is active on your device via
 *  $ systemctl status iotkit-agent
 * If not, activate it with
 *  $ systemctl start iotkit-agent
 *
 * Check the date of your device! It is this date that will be registered
 * in IoT Cloud Analytics.
 */

/*
 * NODE (host) and SERVICE (port)
 * iotkit-agent is listening for UDP data
 * as defined in /etc/iotkit-agent/config.json
 */
#define NODE "localhost"
#define SERVICE "41234"

/*
 * define a component registered on the device
 *  $ iotkit-admin register ${COMP_NAME} ${CATALOG_ID}
 * eg :
 *  $ iotkit-admin register temperature temperature.v1.0
 */
#define COMP_TEMP_F "tempF"
#define COMP_TEMP_C "temp"
#define COMP_LIGHT "light"

static const int UPLOAD_INTERVAL = 5;

int main()
{
	// check that we are running on Galileo or Edison
	mraa_platform_t platform = mraa_get_platform_type();
	if ((platform != MRAA_INTEL_GALILEO_GEN1) &&
			(platform != MRAA_INTEL_GALILEO_GEN2) &&
			(platform != MRAA_INTEL_EDISON_FAB_C)) {
		std::cerr << "Unsupported platform, exiting" << std::endl;
		return MRAA_ERROR_INVALID_PLATFORM;
	}

	// led connected to D3 (digital out)
	upm::GroveLed* led = new upm::GroveLed(3);

	// temperature sensor connected to A0 (analog in)
	upm::GroveTemp* temp_sensor = new upm::GroveTemp(0);

	// LCD connected to the default I2C bus
	upm::Jhd1313m1* lcd = new upm::Jhd1313m1(0);

    // Create the light sensor object using AIO pin 1
    upm::GroveLight* light = new upm::GroveLight(1);

    // Create the button object using GPIO pin 4
    upm::GroveButton* button = new upm::GroveButton(4);

    // Instantiate a rotary sensor on analog pin A3
    upm::GroveRotary* knob = new upm::GroveRotary(3);

    // Instantiate a Grove Loudness sensor on analog pin A2
    upm::GroveLoudness* sound = new upm::GroveLoudness(2);

    // Instantiate a Servo pin D5
    upm::Servo* servo = new upm::Servo(5);

    int chord[] = { DO, RE, MI, FA, SOL, LA, SI, DO, SI };

    // create Buzzer instance
    upm::Buzzer* buzz = new upm::Buzzer(6);

	// selective error checking
	if ((led == NULL) || (temp_sensor == NULL) || (lcd == NULL)) {
		std::cerr << "Can't create all objects, exiting" << std::endl;
		return MRAA_ERROR_UNSPECIFIED;
	}

	// UdpClient class is wrapper for sending UDP data to iotkit-agent
	UdpClient client;
	if (client.connectUdp(NODE, SERVICE) < 0) {
		std::cerr << "Connection to iotkit-agent failed, exiting" << std::endl;
		return MRAA_ERROR_UNSPECIFIED;
	}

	int angle = 0;
	int counter = 0;
	double baselineNoise = sound->value();  // set baseline noise to enviroment at startup

	// loop forever sending the input value periodically
	for (;;) {

		double tempC = temp_sensor->value();; // temperature sensor value in degrees C
		double tempF = tempC * 1.8 + 32;
		double lux = light->value();
	    double loudness = sound->value();
		double knobTurn = (double)knob->abs_deg() / 300;
	    std::stringstream row_1, row_2; 	// LCD rows

		// set lcd color to noise level
		double r = (loudness / baselineNoise - 1) * 255; // adjust as needed  // no red at baseline  // all red at 2x baseline or higher
		if (r > 255) r = 255;
        if (r < 0) r = 0;
		double g = 255 - r; // all green at baseline or lower // no green at 2x baseline or higher
        if (g > 255) g = 255;
        if (g < 0) g = 0;
		double b = 0;

		// change LCD to blue light and set baseline noise to current enviroment
		if (button->value()){
			r = 0;
			g = 0;
			b = knobTurn * 255;
			baselineNoise = sound->value();
		}

		lcd->setColor(r, g, b);

		// display the temperature value on the LCD
		row_1 << "Temp " << tempF << " F        ";
		lcd->setCursor(0,0);
		lcd->write(row_1.str());

		// display the light value on the LCD
		row_2 << "Light " << lux << " lux       ";
		lcd->setCursor(1,0);
		lcd->write(row_2.str());

		lcd->setColor(r, g, b);

		// sound buzz
		if (sound->value() / baselineNoise > 100.0){

			r = 255;
			g = 0;
			lcd->setColor(r, g, b);

			std::string row1 = "KEEPING IT DOWN				";
			std::string row2 = "OH YEAH						";
			lcd->setCursor(0,0);
			lcd->write(row1);
			lcd->setCursor(1,0);
			lcd->write(row2);

			for (int chord_ind = 0; chord_ind < 7; chord_ind++) {
			    // play each note for one second
			    buzz->playSound(chord[chord_ind], 100000);
			}
		}

		lcd->setColor(r, g, b);

		// upload and cout data periodically
		if (counter >= UPLOAD_INTERVAL){

			std::stringstream ssC;
			ssC << "{\"n\":\"" << COMP_TEMP_C << "\",\"v\":" << tempC << "}" << std::endl;	// temp in C
			client.writeData(ssC);

			std::stringstream ssF;
			ssF << "{\"n\":\"" << COMP_TEMP_F << "\",\"v\":" << tempF << "}" << std::endl;		// temp in F
			client.writeData(ssF);

			std::stringstream ssL;
			ssL << "{\"n\":\"" << COMP_LIGHT << "\",\"v\":" << lux << "}" << std::endl;		// light in lux
			client.writeData(ssL);

		    // setup time
			time_t rawtime;
			struct tm * timeinfo;
			time (&rawtime);
			timeinfo = localtime (&rawtime);
            
            // print info
			std::cout << std::endl << asctime(timeinfo);
			std::cout << "temperature: " << tempF << " F / " << tempC << " C " << std::endl;
	        std::cout << "lighting: " << lux << " lux " << std::endl;

	        std::cout<< "sound: " << sound->value() << " / " << baselineNoise << std::endl;
	        std::cout << "button state: " << button->value() << std::endl;	//button state
            fprintf(stdout, "knob state: %5.2f deg \n", knob->abs_deg());

			// blink the led for 50 ms to show the temperature was actually sampled
			led->on();
			usleep(50000);
			led->off();

			// turn servo
			angle = angle + 90;
			servo->setAngle (angle);

			counter = 0;	// reset counter
		}
		counter++;
		sleep(1);
	}

	return MRAA_SUCCESS;
}
