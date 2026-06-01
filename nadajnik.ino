#include <RCSwitch.h>

#define BTN_MORSE 4
#define BTN_PLUS 5
#define BTN_MINUS 3

#define LED_PIN 6
#define BUZZER_PIN 2

RCSwitch mySwitch = RCSwitch();

unsigned long pressStart = 0;
bool pressed = false;

String morseBuffer = "";

unsigned long lastInputTime = 0;

unsigned int morseThreshold = 300;

void setup()
{
    pinMode(BTN_MORSE, INPUT_PULLUP);
    pinMode(BTN_PLUS, INPUT_PULLUP);
    pinMode(BTN_MINUS, INPUT_PULLUP);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    Serial.begin(9600);

    mySwitch.enableTransmit(10);

    Serial.println("Nadajnik gotowy");
}

void loop()
{
    handleSpeedButtons();

    if (digitalRead(BTN_MORSE) == LOW)
    {
        digitalWrite(LED_PIN, HIGH);
        digitalWrite(BUZZER_PIN, HIGH);
    }
    else
    {
        digitalWrite(LED_PIN, LOW);
        digitalWrite(BUZZER_PIN, LOW);
    }

    if (digitalRead(BTN_MORSE) == LOW && !pressed)
    {
        pressStart = millis();
        pressed = true;
    }

    if (digitalRead(BTN_MORSE) == HIGH && pressed)
    {
        unsigned long duration = millis() - pressStart;
        pressed = false;

        if (duration < morseThreshold)
        {
            morseBuffer += ".";
            Serial.print(".");
        }
        else
        {
            morseBuffer += "-";
            Serial.print("-");
        }

        lastInputTime = millis();
    }

    if (morseBuffer.length() > 0 &&
        millis() - lastInputTime > 800)
    {
        char letter = decodeMorse(morseBuffer);

        if (letter >= 'A' && letter <= 'Z')
        {
            int code = letter - 'A' + 1;

            mySwitch.send(code, 8);

            Serial.print(" -> ");
            Serial.println(letter);
        }
        else
        {
            Serial.println("Nieznany kod");
        }

        morseBuffer = "";
    }
}

void handleSpeedButtons()
{
    static unsigned long lastPress = 0;

    if (millis() - lastPress < 200)
        return;

    if (digitalRead(BTN_PLUS) == LOW)
    {
        if (morseThreshold < 1000)
            morseThreshold += 50;

        Serial.print("Prog: ");
        Serial.println(morseThreshold);

        lastPress = millis();
    }

    if (digitalRead(BTN_MINUS) == LOW)
    {
        if (morseThreshold > 100)
            morseThreshold -= 50;

        Serial.print("Prog: ");
        Serial.println(morseThreshold);

        lastPress = millis();
    }
}

char decodeMorse(String code)
{
    if (code == ".-") return 'A';
    if (code == "-...") return 'B';
    if (code == "-.-.") return 'C';
    if (code == "-..") return 'D';
    if (code == ".") return 'E';
    if (code == "..-.") return 'F';
    if (code == "--.") return 'G';
    if (code == "....") return 'H';
    if (code == "..") return 'I';
    if (code == ".---") return 'J';
    if (code == "-.-") return 'K';
    if (code == ".-..") return 'L';
    if (code == "--") return 'M';
    if (code == "-.") return 'N';
    if (code == "---") return 'O';
    if (code == ".--.") return 'P';
    if (code == "--.-") return 'Q';
    if (code == ".-.") return 'R';
    if (code == "...") return 'S';
    if (code == "-") return 'T';
    if (code == "..-") return 'U';
    if (code == "...-") return 'V';
    if (code == ".--") return 'W';
    if (code == "-..-") return 'X';
    if (code == "-.--") return 'Y';
    if (code == "--..") return 'Z';

    return '?';
}