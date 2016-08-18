#include <Arduino.h>
#include <SimpleTimer.h>
#include <SIM900.h>
#include <SoftwareSerial.h>
#include <sms.h>
#include <call.h>
#include <HashMap.h>
#include <string.h>

SoftwareSerial softwareSerial(7, 8);

SMSGSM sms;
CallGSM call;
SimpleTimer timer;

char smsBuffer[160];
char phoneNumber[20];

String requestedDTMF;

boolean connected = false;

HashType<long, char *> hasArray[2];
HashMap<long, char *> subscribersHasMap = HashMap<long, char *>(hasArray, 2);

/**
 * Configure function
 * @return void
 */
void configure() {
    subscribersHasMap[0](131, "+380990977104");
    subscribersHasMap[1](132, "+380668666314");
}

/**
 * Connection to GSM network
 * @return boolean
 */
boolean connectToNetwork() {
    Serial.begin(9600);

    if (gsm.begin(4800)) {
        call.SetDTMF(1);

        return connected = true;
    }

    return false;
}

/**
 * Sends a message to selected number
 * @param phoneNumber
 * @param message_str
 */
void sendSMS(char *phoneNumber, char *message_str) {
    sms.SendSMS(phoneNumber, message_str);
}

/**
 * Rescives SMS from subscribers
 * @return void
 */
void resciveSMS() {
    char position;

    position = sms.IsSMSPresent(SMS_UNREAD);

    if (position) {
        sms.GetSMS(position, phoneNumber, smsBuffer, 20);
    }
}

/**
 * Rescives calls from subscribers
 * @return void
 */
void resciveCall() {
    char dtmf;
    char *phone;

    if (call.CallStatus() == CALL_INCOM_VOICE) {
        delay(3000);
        call.PickUp();
    }

    while (call.CallStatus() == CALL_ACTIVE_VOICE) {
        while (true) {
            dtmf = call.DetDTMF();

            if (dtmf == '-') {
                continue;
            }

            requestedDTMF.concat(dtmf);
            phone = subscribersHasMap.getValueOf(requestedDTMF.toInt());
            Serial.println(phone);

            if (strlen(phone) > 1) {
                sendSMS(phone, "Please let me in!!!");
                break;
            }

            if (requestedDTMF.length() == 3) {
                requestedDTMF = "";
            }
        }
    }
}

void setup() {
    connectToNetwork();
    configure();
}

void loop() {
    resciveCall();
}