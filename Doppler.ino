#include <EEPROM.h>
#include <Arduino.h>
#include <IRremote.hpp>
#define IR_RECEIVE_PIN 2 // To be compatible with interrupt example, pin 2 is chosen here.
#define IR_SEND_PIN 3
#define TONE_PIN 4
#define APPLICATION_PIN 5
#define ALTERNATIVE_IR_FEEDBACK_LED_PIN 6 // E.g. used for examples which use LED_BUILDIN for
example output.
#define _IR_TIMING_TEST_PIN 7
#define RECEIVE_STATE 1
#define SENDING_STATE 0
#define STATE_BUTTON_PIN 10
#define STATE_INDICATOR_LED 12
#define NUMBER_OF_BUTTONS 3
const int BUTTON_LED_PIN[] = { 4,6,8 };
const int BUTTON_PIN[] = { 5,7,9 };
int STATUS_PIN = LED_BUILTIN;
int DELAY_BETWEEN_REPEAT = 500;
int DEFAULT_NUMBER_OF_REPEATS_TO_SEND = 3;
bool remote_state = SENDING_STATE;
bool last_remote_state = RECEIVE_STATE;
// Storage for the recorded code
struct storedIRDataStruct {
IRData receivedIRData;
// extensions for sendRaw
uint8_t rawCode[RAW_BUFFER_LENGTH]; // The durations if raw
uint8_t rawCodeLength; // The length of the code
} sStoredIRData, ir_data[NUMBER_OF_BUTTONS];
void storeCode(IRData *aIRReceivedData);
void sendCode(storedIRDataStruct *aIRDataToSend);
void setup() {
Serial.begin(115200);
// Just to know which program is running on my Arduino
Serial.println(F("START " _FILE_ " from " _DATE_ "\r\nUsing library version " VERSION_IRREMOTE));
// Start the receiver and if not 3. parameter specified, take LED_BUILTIN pin from the internal boards
definition as default feedback LED
IrReceiver.begin(IR_RECEIVE_PIN, ENABLE_LED_FEEDBACK);
IrSender.begin(IR_SEND_PIN, ENABLE_LED_FEEDBACK); // Specify send pin and enable feedback
LED at default feedback LED pin
pinMode(STATUS_PIN, OUTPUT);
Serial.print(F("Ready to receive IR signals of protocols: "));
printActiveIRProtocols (&Serial);
Serial.print(F("at pin "));
Serial.println(IR_RECEIVE_PIN);
Serial.print(F("Ready to send IR signals at pin "));
Serial.print(IR_SEND_PIN);
// Serial.print(F(" on press of button at pin "));
// Serial.println(SEND_BUTTON_PIN);
// receiver will always stay on
IrReceiver.start();
pinMode(STATE_BUTTON_PIN, INPUT);
for(int button=0; button<NUMBER_OF_BUTTONS; button++) {
pinMode(BUTTON_PIN[button], INPUT);
pinMode(BUTTON_LED_PIN[button], INPUT);
EEPROM.get(sizeof(storedIRDataStruct)*button, ir_data[button]);
Serial.print("button no: :");
Serial.println(button);
Serial.println(ir_data[button].rawCodeLength);
}
}
void loop() {
// state button pin is the pushed state pin -- check if its needed to do internal pullup
if( digitalRead(STATE_BUTTON_PIN) ) {
remote_state = RECEIVE_STATE;
}else {
remote_state = SENDING_STATE;
}
// turning on receiver on state switch
if (last_remote_state == SENDING_STATE && remote_state == RECEIVE_STATE) {
// Re-enable receiver
Serial.println(F("receiver started"));
IrReceiver.start();
// remember to do irreceiver stop at end?
}
if (last_remote_state == RECEIVE_STATE && remote_state == SENDING_STATE) {
Serial.println(F("sender started"));
IrReceiver.stop();
}
// taking button readings
bool button_state[] = {
digitalRead(BUTTON_PIN[0]),
digitalRead(BUTTON_PIN[1]),
digitalRead(BUTTON_PIN[2]),
};
int pressed_button = -1;
for(int i=0; i<NUMBER_OF_BUTTONS; i++) {
if( button_state[i] ) {
pressed_button = i;
break;
}
}
// Serial.println(remote_state);
if( remote_state == RECEIVE_STATE ) {
if (IrReceiver.available() && pressed_button != -1) {
digitalWrite(BUTTON_LED_PIN[pressed_button], HIGH);
storeCode(IrReceiver.read(), pressed_button);
Serial.println(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
IrReceiver.resume(); // resume receiver
Serial.println("raw code length");
Serial.println(ir_data[pressed_button].rawCodeLength);
delay(1000);
digitalWrite(BUTTON_LED_PIN[pressed_button], LOW);
}
}else {
if (pressed_button != -1) {
IrReceiver.stop();
Serial.println(F("Button pressed, now sending"));
digitalWrite(STATUS_PIN, HIGH);
if (last_remote_state == SENDING_STATE) {
ir_data[pressed_button].receivedIRData.flags = IRDATA_FLAGS_IS_REPEAT;
}
sendCode(&ir_data[pressed_button]);
digitalWrite(STATUS_PIN, LOW);
delay(DELAY_BETWEEN_REPEAT); // Wait a bit between retransmissions
}
}
last_remote_state = remote_state;
}
// Stores the code for later playback in sStoredIRData
// Most of this code is just logging
void storeCode(IRData *aIRReceivedData, int button) {
if (aIRReceivedData->flags & IRDATA_FLAGS_IS_REPEAT) {
Serial.println(F("Ignore repeat"));
return;
}
if (aIRReceivedData->flags & IRDATA_FLAGS_IS_AUTO_REPEAT) {
Serial.println(F("Ignore autorepeat"));
return;
}
if (aIRReceivedData->flags & IRDATA_FLAGS_PARITY_FAILED) {
Serial.println(F("Ignore parity error"));
return;
}
/*
* Copy decoded data
*/
ir_data[button].receivedIRData = *aIRReceivedData;
if (ir_data[button].receivedIRData.protocol == UNKNOWN) {
Serial.print(F("Received unknown code and store "));
Serial.print(IrReceiver.decodedIRData.rawDataPtr->rawlen - 1);
Serial.println(F(" timing entries as raw "));
IrReceiver.printIRResultRawFormatted(&Serial, true); // Output the results in RAW format
ir_data[button].rawCodeLength = IrReceiver.decodedIRData.rawDataPtr->rawlen - 1;
IrReceiver.compensateAndStoreIRResultInArray(ir_data[button].rawCode);
} else {
IrReceiver.printIRResultShort(&Serial);
ir_data[button].receivedIRData.flags = 0; // clear flags -esp. repeat- for later sending
Serial.println();
}
Serial.println("GGGGGGGGGGGGGGGGGGGGGGGGGGG");
digitalWrite(BUTTON_LED_PIN[button], HIGH);
EEPROM.put(sizeof(storedIRDataStruct)*button, ir_data[button]);
delay(1000);
digitalWrite(BUTTON_LED_PIN[button], LOW);
}
void sendCode(storedIRDataStruct *aIRDataToSend) {
if (aIRDataToSend->receivedIRData.protocol == UNKNOWN ) {
// Assume 38 KHz
IrSender.sendRaw(aIRDataToSend->rawCode, aIRDataToSend->rawCodeLength, 38);
Serial.print(F("Sent raw "));
Serial.print(aIRDataToSend->rawCodeLength);
Serial.println(F(" marks or spaces"));
} else {
/*
* Use the write function, which does the switch for different protocols
*/
IrSender.write(&aIRDataToSend->receivedIRData, DEFAULT_NUMBER_OF_REPEATS_TO_SEND);
Serial.print(F("Sent: "));
printIRResultShort(&Serial, &aIRDataToSend->receivedIRData);
}