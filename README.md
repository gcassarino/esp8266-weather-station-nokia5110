# esp8266 weather station for Nokia 5110 LCD

esp8266 weather station for Nokia 5110 monochrome LCD display using <a href="https://github.com/squix78/esp8266-weather-station">Daniel Eichhorn's (squix78)</a> library.

This code uses the Adafruit GFX libraries and its PCD8544 library to drive the Nokia display.
I wrote a modified version of Daniel's esp8266-oled-ssd1306's Ui Library (OLEDDisplayUi) - a version adapted to work with the Adafruit driver - removing some parts related to the active / inactive frame indicator images (setActiveSymbol / setInactiveSymbol) as I did not need it.
For more information, see https://github.com/squix78/esp8266-oled-ssd1306#ui-library-oleddisplayui

The following information are displayed:

* Time and date
* Current weather and today forecast details
* Forecasts over a 3 day period after today
* Moon details: phase, rise/fall time, luminosity, age
* Sun and wind details: sun rise/fall time, wind speed and direction

The date and time are obtained using the WundergroundClient module from the library, I found it easier to use instead of the NTPClient module because the latter does not handle the daylight saving time (DST) while the WundergroundClient already returns the correct DST from the country defined in the code. This way I could use just WundergroundClient module for all functions.

Finally in place of the images with the weather icons I used a font with other weather icons (see credits below). Fonts icons can be edited with any font editor. I inserted the project file created with [Birdfont](https://birdfont.org/) in another repository TODO.



## Getting Started TODO

These instructions will get you a copy of the project up and running on your local machine for development and testing purposes. See deployment for notes on how to deploy the project on a live system.
* wiring, breadboard, pins
* esp8266 wemos
* screenshots

### Installing

this code is a PlatformIO project, open the project from PlatformIO.


## Authors

* **Daniel Eichhorn** - [squix78](https://github.com/squix78) - *Initial work*
* Gianluca Cassarino - this adaptation for Nokia 5110 LCD display

See also the list of [contributors](https://github.com/gcassarino/esp8266-weather-station-nokia5110/contributors) who participated in this project.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details

## Acknowledgments/Credits

* **Daniel Eichhorn** [(squix78)](https://github.com/squix78) for writing the excellent library.
* [Adafruit Industries](https://www.adafruit.com/)
* Original Weather icon set font from [PIXEDEN](https://www.pixeden.com/icon-fonts/the-icons-font-set-weather)
* Nokia font from [DAFONT](https://www.dafont.com/nokia-cellphone.font)
* please look at the [resources related repository](https://github.com/gcassarino/weather-station-nokia5110-resources) for the original copies of licensed works.
