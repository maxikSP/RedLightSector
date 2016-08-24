#include <Arduino.h>
#include <SIM900.h>
#include <SoftwareSerial.h>
#include <sms.h>
#include <call.h>
#include <Keypad.h>
#include <SPI.h>
#include <MFRC522.h>

#define SS_PIN 10
#define RST_PIN 6

// Debug mode. Comment it if debug don.t needed
#define DEBUG_MODE;

SoftwareSerial softwareSerial(7, 8);
MFRC522 rFid(SS_PIN, RST_PIN);
MFRC522::MIFARE_Key key;

SMSGSM sms;
CallGSM call;

char smsBuffer[160];
char incomePhone[20];

String requestedDTMF;
String requestedPosition;

boolean connected = false;

const byte FIRST_POS = 1;
const byte LAST_POS = 250;

const byte ROWS = 4;
const byte COLS = 4;

char keymap[ROWS][COLS] = {
    { '1', '2', '3', 'A' },
    { '4', '5', '6', 'B' },
    { '7', '8', '9', 'C' },
    { '*', '0', '#', 'D' }
};

byte rowPins[ROWS] = {A0, A1, A2, A3};
byte colPins[COLS] = {5, 4, 3, 2};
byte defaultKeys[1][MFRC522::MF_KEY_SIZE] = {
    { 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF }
};

byte dataBlock[2][16] = {
    {
         0x42, 0x07, 0x68, 0x89, // 66, 7, 104, 137,
         0xA,  0x06, 0x47, 0x52, // 10, 6, 71, 82,
         0x33, 0x09, 0x7F, 0x0C, // 51, 9, 127, 12,
         0x62, 0x83, 0x6B, 0x10  // 98, 131, 107, 16
    },
    {
         0x6, 0x6D, 0x31, 0x58, // 6, 109, 49, 88
         0x3, 0x57, 0x1, 0x2C,  // 3, 87, 1, 44
         0x65, 0x35, 0x5, 0x6,  // 101, 53, 5, 6
         0x91, 0x5B, 0x14, 0x2  // 145, 91, 20, 2
    }
};

Keypad keypad = Keypad(makeKeymap(keymap), rowPins, colPins, sizeof(rowPins), sizeof(colPins));

/**
 * Connection to GSM network
 */
boolean connectToNetwork() {
    pinMode(A0, OUTPUT);
    pinMode(A1, OUTPUT);
    pinMode(A2, OUTPUT);
    pinMode(A3, OUTPUT);
    pinMode(A4, OUTPUT);
    pinMode(A5, OUTPUT);
    pinMode(9, OUTPUT);

    digitalWrite(9, HIGH);
    Serial.begin(9600);

    if (gsm.begin(9600)) {
        call.SetDTMF(1);
        SPI.begin();
        rFid.PCD_Init();

        for (byte i = 0; i < MFRC522::MF_KEY_SIZE; i++) {
            key.keyByte[i] = defaultKeys[0][i];
        }

        connected = true;
    }

    return connected;
};

/**
 * Returns phone number position in SIM memoty.
 */
byte findPhonePosition(char phone[20]) {
    char tmpPhone[20];

    for (byte i = 0; i < LAST_POS; i++) {
        gsm.GetPhoneNumber(i, tmpPhone);

        if (tmpPhone == phone) {
            return i;
        }
    }

    return 0;
};

/**
 * Opens the dore
 */
void openTheDore(boolean calling = true) {
    digitalWrite(9, LOW);
    delay(500);
    digitalWrite(9, HIGH);
    delay(500);

    if (calling) {
        call.HangUp();
    }
};

/**
 * Rescive sms from subscribers
 */
void resciveSMS() {
    char position = sms.IsSMSPresent(SMS_UNREAD);

    if (position) {
        char status = sms.GetAuthorizedSMS((byte) position, incomePhone, smsBuffer, 160, FIRST_POS, LAST_POS);
    }
};

/**
 * Rescives DTMF from subscribers
 */
void resciveCallDTMF(int pos) {
    char dtmf;

    while (call.CallStatus() == CALL_ACTIVE_VOICE) {
        while (true) {
            if (call.CallStatus() == CALL_NONE) {
                break;
            }

            dtmf = call.DetDTMF();

            if (dtmf == '-') {
                continue;
            }

            requestedDTMF.concat(dtmf);

            #ifdef DEBUG_MODE
                Serial.print(F("Requested DTMF signal:"));
                Serial.println(requestedDTMF);
            #endif

            openTheDore();
            requestedDTMF = "";

//            if (requestedDTMF.toInt() == pos) {
//                openTheDore();
//                requestedDTMF = "";
//                break;
//            }
//
//            if (requestedDTMF.length() == 3) {
//                requestedDTMF = "";
//            }
        }
    }
};

/**
 * Unlock dore from subscribers
 */
void resciveCall() {

    byte status = call.CallStatusWithAuth(incomePhone, FIRST_POS, LAST_POS);

    if (status == CALL_INCOM_VOICE_AUTH) {
        openTheDore();
    }
};

/**
 * Contact subscriber to open the dore
 */
void makeCall(int position) {
    call.Call(position);
    resciveCallDTMF(position);
};

/**
 * Helper routine to dump a byte array as hex values to Serial.
 */
void dump_byte_array(byte *buffer, byte bufferSize) {
    for (byte i = 0; i < bufferSize; i++) {
        Serial.print(buffer[i] < 0x10 ? " 0" : " ");
        Serial.print(buffer[i], HEX);
    }
}

/**
 * Check for RFID authentication
 */
void resciveRFIDKey(byte *keys) {
    byte PICCType;
    byte bytesBuffer[18];
    byte bufferSize = sizeof(bytesBuffer);
    byte blockAddr = 4;
    MFRC522::StatusCode status;
    int match = 0;

    if (!rFid.PICC_IsNewCardPresent() || !rFid.PICC_ReadCardSerial()) {
        return;
    }

    #ifdef DEBUG_MODE
        PICCType = rFid.PICC_GetType(rFid.uid.sak);


        Serial.print(F("Card type:"));
        Serial.println(PICCType);

        if (PICCType != MFRC522::PICC_TYPE_MIFARE_MINI
            &&  PICCType != MFRC522::PICC_TYPE_MIFARE_1K
            &&  PICCType != MFRC522::PICC_TYPE_MIFARE_4K) {
            Serial.println(F("This sample only works with MIFARE Classic cards."));
            return;
        }
    #endif

    status = rFid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, 7, &key, &(rFid.uid));

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
        return;
    }

    status = rFid.MIFARE_Read(blockAddr, bytesBuffer, &bufferSize);

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("MIFARE_Read() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
        return;
    }

    for (byte i = 0; i < sizeof(keys); i++) {
        if (keys[i] == bytesBuffer[i]) {
            match++;
        }
    }

    #ifdef DEBUG_MODE
        dump_byte_array(bytesBuffer, bufferSize);

        Serial.print(F("Bytes was match amount:"));
        Serial.println(match);

        Serial.print(F("Size of initial bytes:"));
        Serial.println(sizeof(keys));
    #endif

    if (match != sizeof(keys)) {
        #ifdef DEBUG_MODE
            Serial.println(F("Access denied"));
        #endif
    } else {
        #ifdef DEBUG_MODE
            Serial.println(F("Access granted"));
        #endif
        openTheDore(false);
    }

    rFid.PICC_HaltA();
    rFid.PCD_StopCrypto1();
}

/**
 * Writes new card data.
 * This feature disabled by default.
 */
void writeNewRFIDKey(byte *keys) {

    if (!rFid.PICC_IsNewCardPresent() || !rFid.PICC_ReadCardSerial()) {
        return;
    }

    #ifdef DEBUG_MODE
        // Show some details of the PICC (that is: the tag/card)
        Serial.print(F("Card UID:"));
        Serial.println();
        Serial.print(F("PICC type: "));
    #endif

    MFRC522::PICC_Type piccType = rFid.PICC_GetType(rFid.uid.sak);

    #ifdef DEBUG_MODE
        Serial.println(rFid.PICC_GetTypeName(piccType));
    #endif

    // Check for compatibility
    if (piccType != MFRC522::PICC_TYPE_MIFARE_MINI
            &&  piccType != MFRC522::PICC_TYPE_MIFARE_1K
            &&  piccType != MFRC522::PICC_TYPE_MIFARE_4K) {
        #ifdef DEBUG_MODE
                Serial.println(F("This sample only works with MIFARE Classic cards."));
        #endif
        return;
    }

    // In this sample we use the second sector,
    // that is: sector #1, covering block #4 up to and including block #7
    byte sector = 1;
    byte blockAddr = 4;
    byte trailerBlock = 7;
    MFRC522::StatusCode status;
    byte buffer[18];
    byte size = sizeof(buffer);

    #ifdef DEBUG_MODE
        // Authenticate using key A
        Serial.println(F("Authenticating using key A..."));
    #endif

    status = rFid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_A, trailerBlock, &key, &(rFid.uid));

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
        return;
    }

    #ifdef DEBUG_MODE
        // Show the whole sector as it currently is
        Serial.println(F("Current data in sector:"));
        rFid.PICC_DumpMifareClassicSectorToSerial(&(rFid.uid), &key, sector);
        Serial.println();
    #endif

    #ifdef DEBUG_MODE
        // Read data from the block
        Serial.print(F("Reading data from block "));
        Serial.print(blockAddr);
        Serial.println(F(" ..."));
    #endif

    status = rFid.MIFARE_Read(blockAddr, buffer, &size);

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("MIFARE_Read() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
    }

    #ifdef DEBUG_MODE
        Serial.print(F("Data in block "));
        Serial.print(blockAddr);
        Serial.println(F(":"));
        Serial.println();
    #endif

    #ifdef DEBUG_MODE
        // Authenticate using key B
        Serial.println(F("Authenticating again using key B..."));
    #endif

    status = rFid.PCD_Authenticate(MFRC522::PICC_CMD_MF_AUTH_KEY_B, trailerBlock, &key, &(rFid.uid));

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("PCD_Authenticate() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
        return;
    }

    #ifdef DEBUG_MODE
        // Write data to the block
        Serial.print(F("Writing data into block "));
        Serial.print(blockAddr);
        Serial.println(F(" ..."));
    #endif

    status = rFid.MIFARE_Write(blockAddr, keys, 16);

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("MIFARE_Write() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
    }

    #ifdef DEBUG_MODE
        Serial.println();

        // Read data from the block (again, should now be what we have written)
        Serial.print(F("Reading data from block "));
        Serial.print(blockAddr);
        Serial.println(F(" ..."));
    #endif

    status = rFid.MIFARE_Read(blockAddr, buffer, &size);

    if (status != MFRC522::STATUS_OK) {
        #ifdef DEBUG_MODE
            Serial.print(F("MIFARE_Read() failed: "));
            Serial.println(rFid.GetStatusCodeName(status));
        #endif
    }

    #ifdef DEBUG_MODE
        Serial.print(F("Data in block "));
        Serial.print(blockAddr);
        Serial.println(F(":"));

        // Check that data in block is what we have written
        // by counting the number of bytes that are equal
        Serial.println(F("Checking result..."));

        byte count = 0;

        for (byte i = 0; i < 16; i++) {
            // Compare buffer (= what we've read) with keys (= what we've written)
            if (buffer[i] == keys[i])
                count++;
        }

        Serial.print(F("Number of bytes that match = "));
        Serial.println(count);

        if (count == 16) {
            Serial.println(F("Success :-)"));
        } else {
            Serial.println(F("Failure, no match :-("));
            Serial.println(F("  perhaps the write didn't work properly..."));
        }

        Serial.println();

        // Dump the sector data
        Serial.println(F("Current data in sector:"));
        rFid.PICC_DumpMifareClassicSectorToSerial(&(rFid.uid), &key, sector);
        Serial.println();
    #endif

    rFid.PICC_HaltA();
    rFid.PCD_StopCrypto1();
}

/**
 * Key press event handler
 */
void _onKeyPressed(KeypadEvent target) {
    switch (keypad.getState()) {
        case PRESSED:
            requestedPosition.concat(target);

            #ifdef DEBUG_MODE
                Serial.print(F("Keys pressed:"));
                Serial.println(requestedPosition);
            #endif

            if (target == '#') {
                call.HangUp();
                requestedPosition = "";
            } else if (target == '*') {
                makeCall((int) requestedPosition.toInt());
                requestedPosition = "";
            } else if (requestedPosition.length() == 3) {
                requestedPosition = "";
            }

            break;

    }
};

void setup() {
    connectToNetwork();
    keypad.addEventListener(_onKeyPressed);
};

void loop() {
    keypad.getKey();
    resciveCall();
    resciveRFIDKey(dataBlock[0]);
//    writeNewRFIDKey(dataBlock[0]);
};