# Desk Assist
## A device for automated desk environment comfort and control.
### Overview

This project was is a culmination of my desire to explore embedded system design and electrical components that I found in my house over winter break. The device, equiped with a fan, vent system, and lamp bulb was built with automated comfort in mind. Utilizing a temperature sensor and photoresistor, the device will turn the fan and vent on if the room is a certain temperature and will turn the lamp on if the room is dark. The device also is built with an OLED user interface where a user can customize fan speed, vent angle, and lamp brightness. The user can also toggle auto functionality for a given sensor on or off for a more customizable experience.

****

### What I Learned

Building Desk Assist from the ground up taught me a lot about embedded system software and electrical design.

**The Software:**

I learned how to configure drivers designed for an MCU and build device drivers by reading driver and component documentation. I used FreeRTOS to create an embedded system that reacted to external factors in real time using a combination of tasks, ISRs, queues to transfer data, and mutexes to protect shared resources. 

While figuring out how to implement ESP-IDFs SPI drivers I also learned about board communication protocols like UART, I2C, and SPI. I learned the pros/cons of each and in which situations they are used. While discovering how to write a ESP32 WiFi driver I also gained exposure to netorking stacks in c and how different WiFi protocols like HTTP work.

I also learned how to use tools like CMake and Kconfig to build modular code that is easily configurable within the build environment.

In the future I will refine my device drivers to have the same customization as the WiFi driver so they can be ported to future projects with minimal change.

Hardware:

When wiring components onto 3 different breadboards, I learned the importance of component organization. I found that organizing components relative to their powersource, board pin, and other related components is important for ease of device use and electrical debugging.

When desigining circuits, I accounted for electrical noise using a filtering capacitor at the terminals of the 9V battery, accross the DC motor terminals, and on ADC pins. I also accounted for voltage spikes from the DC motor with a flyback diode. I found it interesting how much hardware affects the preformance of devices in ways that software can't. For instance, the servo driving the vent system stuttered when the DC motor was activated with no filtering capacitor attached.

I utilized a multimeter for most debugging when testing for expected behavior with GPIO and ADC pins. I also made sure to test the max voltage on any input pins to make sure I did not apply any voltage over 3.3V to my ESP32 chip.


****

### Functionality

This project was built by using espidf to program an ESP32 DevKit C. FreeRTOS was used to create an embedded system that could effectively interact with the user and the environment in real time.

**User Interface:**

The user interacts with this device through an OLED screen that is controlled by two buttons. The default display is a home screen that displays the inside temperature taken by the TMP36, the outside temperature gathered from an HTTP request, and the current time which is synced with the real world using SNTP. When any button is pressed a menu system is displayed an the user can descend into the menu system to modify the power given to a device, turn auto mode on/off, or disable the device completely. The logic for the display is handled by a `user_interface_task()` which sends data to the `controller_task()` using a queue based on what the user selected.

**Sensor Data:**

The buttons use GPIO interrupts to trigger an ISR that simply sends a message to the `user_interface_task()`, telling it which button was pressed. Button debouncing was handled by measuring the time between clicks. 

The potentiometer is used to adjust the pwm delivered to actuators. When the user selectes "Adjust" on the menu for a given actuator, the `controller_task()` starts a hardware timer to trigger `pot_signal_callback()` which tells `potentiometer_task()` to unblock and sample the potentiometer voltage using an ADC channel. This value is output from the potentiometer driver as a percentage of its max value and fed into whichever actuator value the user is modifying. When the user chooses to stop adjusting, the hardware timer is stopped.

The TMP36 and photoresistor are both measured periodically through `read_temp_photo()` which uses a delay function to avoid hogging the cpu. Data is first read as percentages of max values for each sensor using the ADC and then sent to the `controller_task()` to be fed into device drivers. Temperature is also read in celcius to be sent to the `user_interface_task()` to be displayed on the home screen. 

Any measurement that uses the ADC requires a mutex to access the ADC driver as all ADC channels are located on one unit and cannot be measured concurrently. 

**Actuators:**

This project has 4 main outputs besides the OLED screen: a servo controlled vent, DC motor fan, lamp bulb, and an 8 bit shift register. The first 3 actuators are controlled using PWM. The lamp and DC motor utilize a logic level N-Mosfets as they require larger voltages than 3.3V and don't have a power cable like the servo. The shift register controlls 5 LEDs and is used to indicate the "level" of the potentiometer as the user is adjusting an actuator. It behaves like a progress bar where if the potentiometer is at it's lowest, no LEDs are lit and if the potentiometer is at its max, all 5 will be lit.


**Device Drivers:**

All actuators were constructed with the same functionality in mind. They all have internal handling for toggling the system on/off and turning automatic mode on/off. Boolean variables keep track of these states and determin when duty cycle updates are pushed to the esp_ledc driver. Data input by the user using the adjusting potentiometer will always be stored. An auto_on boolean is kept to store whether input sensor readings meet the conditions for an actuator to be turned on. If auto mode is turned on, the drivers will handle turning on or off each time sensor data is fed in.

The display was composed using u8g2 and an ESP-IDF HAL. The display driver has a premade homescreen that takes in inputs for inside temperature, outside temperature, and time. It also has a menu system that uses structs to print out menu items and a selection cursour.

The 8 Bit Shift Register IC communication is done using spi and gpio drivers to deliver bit sequences that indicate when the given spi communicated values should be pushed to the ICs outputs.

**Wifi:**

Coming

****

Components Used:
- OLED Screen
- 2 Buttons
- Servo Motor
- 9V DC Motor
- 3.6v Lamp Bulb
- 2 IRLZ44N Mosfets
- 8 Bit Shift Register IC
- 5 LEDS
- Potentiometer
- TMP36
- Photoresistor
- External 9V Battery
- Capacitors, Resistors, and many Wires




