/************************************************************************************/
/************************************************************************************/
/* Settings -   BEGIN                                                               */
/************************************************************************************/
/************************************************************************************/

const char* ssid = "xxx";
const char* password = "xxx";
const char* hostname = "Doorbell";
const char* otaPassword = "xxx";
const char* MqttBroker = "192.168.x.x";
const char* MqttTopicPrefix = "Doorbell";
//const char* MqttSetTopicRing = "Doorbell/SetRing";

// Activate sending log_parameters to Mqtt broker every log_interval seconds
#define MqttBrokerIP 192,168,x,x	// Please use commas instead of dots!!!
//#define MqttUsername "User" // Set username for Mqtt broker here or comment out if no username/password is used.
//#define MqttPassword "Pass" // Set password for Mqtt broker here or comment out if no password is used.
//#define MqttTopicPrefix "Doorbell" 	// Optional: Choose the "topic" for Mqtt messages here
//#define MqttJson 					// Optional: Use this if you want a json package of your logging information printed to the Mqtt topic
//#define MqttUnit           // Optional: Use this if you want a json package including units in the value
//#define MqttDeviceID "Doorbell"	// Optional: Define a device name to use as header in json payload. If not defined, BSB-LAN will be used.
#define MqttSubscriptionPrefix "fhem" // Optional: Topic for listening
#define MqttSetTopicRing "fhem/Doorbell/Ring" // Optional: Topic for setting the level of the ventilation

// Define GPIOs
#define RELAY 0 // relay connected to  GPIO0
#define BELLBUTTON 2 // relay connected to  GPIO2

// Time the relay is active (milliseconds)
int ringTime = 50;