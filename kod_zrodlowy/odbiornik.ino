#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RCSwitch.h>

#define LED_PIN 6
//#define BUZZER_PIN 3

LiquidCrystal_I2C lcd(0x27, 16, 2);

RCSwitch mySwitch = RCSwitch();

int cursorPos = 0;
int receivedCount = 0;

void setup()
{
    Serial.begin(9600);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Odbiornik");

    mySwitch.enableReceive(0);

    Serial.println("Nasluch...");
}

void loop()
{
    if (mySwitch.available())
    {
        int value = mySwitch.getReceivedValue();

        if (value >= 1 && value <= 26)
        {
            char letter = 'A' + value - 1;

            Serial.print("RX: ");
            Serial.println(letter);

            showLetter(letter);

            digitalWrite(LED_PIN, HIGH);

            tone(BUZZER_PIN, 1000);
            delay(100);
            noTone(BUZZER_PIN);

            digitalWrite(LED_PIN, LOW);

            receivedCount++;

            lcd.setCursor(0, 0);
            lcd.print("CNT:");
            lcd.print(receivedCount);
            lcd.print("   ");
        }

        mySwitch.resetAvailable();
    }
}

void showLetter(char letter)
{
    lcd.setCursor(cursorPos, 1);
    lcd.print(letter);

    cursorPos++;

    if (cursorPos >= 16)
    {
        cursorPos = 0;

        lcd.setCursor(0, 1);
        lcd.print("                ");
    }
}