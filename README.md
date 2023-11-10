![Project Logo](https://github.com/circuito-suman/BinaryInternetClock/blob/main/Pictures/Layer%201.png?raw=true) 

**WiFi Binary Clock** is an ESP8266-based clock that displays the current time in a captivating binary format while staying synchronized with an online time server. This repository contains the code and resources you need to create your own unique WiFi Binary Clock.

## Features
- **Binary Time Representation**: The clock visually represents time using a grid of LED lights in binary form for hours, minutes, and seconds.
- **Internet Connectivity**: Automatically fetches accurate time data from an online NTP server.
- **Customization**: Customize your clock's appearance, color schemes, and LED animations to match your style.
- **ESP8266-Based**: Suitable for both beginners and experienced Arduino enthusiasts.
- **User-Friendly Interface**: Easy-to-use interface for configuring your clock's settings.
- **Brightness Control**: Automatically adjusts brightness based on the environment.
- **Auto-Reboot**: Periodic reboot for stable operation.
- **Open Source**: Contribute, modify, and adapt the code and hardware design to create your variations.

## Build
To build your own WiFi Binary Clock, follow these steps:

1. Clone or download this repository to your local machine.
2. Wire the components based on the provided circuit diagram.
3. Set your WiFi network SSID and password in the code.
4. Upload the code to your Arduino-compatible microcontroller.
5. Power up your clock and enjoy!

## Usage
Here's how to use your WiFi Binary Clock:

1. Connect the clock to your WiFi network.
2. The clock will automatically synchronize with an NTP server.
3. Watch the mesmerizing LED lights display the current time in binary format.

```cpp
// Example code for setting WiFi SSID and password
char ssid[] = "your_network_SSID";
char pass[] = "your_network_password";
```

## Photos and Circuit Diagram

![App Screenshot](https://github.com/circuito-suman/BinaryInternetClock/blob/main/Pictures/CircuitDiagram.png?raw=true)

![App Screenshot](https://github.com/circuito-suman/BinaryInternetClock/blob/main/Pictures/1.jpg?raw=true)

![App Screenshot](https://github.com/circuito-suman/BinaryInternetClock/blob/main/Pictures/2.jpg?raw=true)

![App Screenshot](https://github.com/circuito-suman/BinaryInternetClock/blob/main/Pictures/3.jpg?raw=true)

![App Screenshot](https://github.com/circuito-suman/BinaryInternetClock/blob/main/Pictures/4.jpg?raw=true)

## Clock working video
Check out this video of my project in action on Instagram:

[![Instagram Video](https://img.shields.io/badge/Instagram-Video-red?logo=instagram&style=for-the-badge)](https://www.instagram.com/p/Cmil8wFDJDa/)


## Environmental Variables

To configure your WiFi Binary Clock, you can modify the following environmental variables in the code:

```cpp
// NTP server settings
const char* ntpServerName = "in.pool.ntp.org";

// LED control pins
int latchPin = 4;
int clockPin = 5;
int dataPin = 14;

// Timezone correction and reboot interval
double timeZoneCorrection = 5.30;
const int rebootEvery = 60 * 60 * 24; // Reboot every 24 hours

// Brightness control
brightness.attach_ms(25, bright);
```
## Acknowledgements

The WiFi Binary Clock project is built upon the work of the Arduino and ESP8266 communities, as well as the NTP Client example for ESP8266. Special thanks to these communities for their open-source contributions.

Feel free to contribute, report issues, or suggest improvements to make this project even better.

Happy clock-making!


## About Me

👋 Hi, I'm Suman Saha, a Computer Science Engineering student passionate about technology and coding.

### Connect with Me

[![GitHub](https://img.shields.io/badge/GitHub-circuito-suman-brightgreen?logo=github&style=for-the-badge)](https://github.com/circuito-suman)

[![Instagram](https://img.shields.io/badge/Instagram-circuito_suman-red?logo=instagram&style=for-the-badge)](https://www.instagram.com/circuito_suman/)

[![LinkedIn](https://img.shields.io/badge/LinkedIn-SumanSaha-blue?logo=linkedin&style=for-the-badge)](http://www.linkedin.com/in/suman-saha-69ba5029a)


Feel free to reach out and connect with me on GitHub, Instagram and  LinkedIn!
