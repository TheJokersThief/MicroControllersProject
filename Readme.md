# Burglar Alarm Arduino Project
**Task:** Build a burglar alarm — The task for the project was to build a device, using an arduino, that could monitor several different types of zones, log alarm trips and allow for user and admin interaction via an LCD. When an alarm condition is met, a buzzer or LED will go off indicating as such.

## Authors
1. **Evan Smith**
	* [iamevan.me](http://iamevan.me)
	* [@TheJokersThief](http://twitter.com/thejokersthief)
	* [evan.netsoc.co](http://evan.netsoc.co)
2. **Colm Cahalane**
	* [colm.cf](http://colm.cf)
	* [@1108oo](http://twitter.com/1108oo)
	* [colm.netsoc.co](http://colm.netsoc.co)

## Limitations
* 512MB permanent memory for logging and settings storage 
* Only the admin is allowed to change settings
 	
## Features
* The date and time is displayed by default on the LCD 
* There are 4 zones hooked up to the alarm:
	1. Entry/Exit: This is linked to the front door. When tripped, it allows a certain amount of time to disable the alarm before it starts ringing.
	2. Digital: This would be linked to sensors attached to the windows or other vulnerable aspects of the house. When contact is broken, it sets off the alarm. E.G. If a burglar broke in by prying open the window.
	3. Analog: Connected to analog sensors such as a thermometer for detecting heat or variable motion sensors. This is tripped after the sensor signal breaches a certain threshold.
	4. Continuous Monitoring: This alarm is tripped when the signal transitions from high to low, used primarily to ensure no tampering occurs with the burglar alarm.
* A number of zones are programmable and administrators can set a variety of options like: – User password
	* Admin password
	* Do-not-disturb times for the entry/exit zone – Digital zone trip condition
* Whenever a zone is tripped, the event is logged to permanent storage
* Settings, logs, passwords and alarm configuration can be done with an Infra Red Remote