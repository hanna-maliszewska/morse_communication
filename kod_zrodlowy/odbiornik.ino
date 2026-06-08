#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RCSwitch.h>

static const uint8_t LED_PIN = 6U;
static const uint8_t BTN_CLEAR_PIN = 8U;
static const uint8_t LCD_ADDR = 0x27U;
static const uint8_t LCD_COLS = 16U;
static const uint8_t LCD_ROWS = 2U;

static LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
static RCSwitch mySwitch = RCSwitch();

static uint8_t cursorPos = 0U;
static uint16_t receivedCount = 0U;

static void showLetter(char letter);

/*!
 * @brief    Inicjalizuje piny wejscia/wyjscia, port szeregowy, wyswietlacz LCD oraz modul odbiornika radiowego.
 * @param    brak parametrow
 * brak
 * @returns  brak
 * @side effects:
 * Konfiguruje wyjscia i wejscia mikrokontrolera, inicjalizuje magistrale I2C dla LCD, 
 * uruchamia przerwania zewnetrzne dla nasluchu radiowego.
 */
void setup()
{
    Serial.begin(9600);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_CLEAR_PIN, INPUT_PULLUP);

    lcd.init();
    lcd.backlight();

    lcd.setCursor(0, 0);
    lcd.print("Odbiornik");

    mySwitch.enableReceive(0);

    Serial.println("Nasluch...");
}

/*!
 * @brief    Glowna petla programu obslugujaca czyszczenie ekranu przyciskiem oraz odbior wiadomosci radiowych.
 * @param    brak parametrow
 * brak
 * @returns  brak
 * @side effects:
 * Modyfikuje stan wyswietlacza LCD (w tym czysci go), resetuje i inkrementuje licznik 
 * odebranych wiadomosci (receivedCount), zmienia stan diody LED na krotki czas.
 */
void loop()
{
    if (digitalRead(BTN_CLEAR_PIN) == LOW)
    {
        lcd.clear();
        
        lcd.setCursor(0, 0);
        lcd.print("Odbiornik");
        
        cursorPos = 0U;
        receivedCount = 0U;
        
        Serial.println("Ekran i licznik wyczyszczone.");
        
        delay(300U); 
    }

    if (mySwitch.available())
    {
        uint32_t value = (uint32_t)mySwitch.getReceivedValue();

        if ((value >= 1U) && (value <= 26U))
        {
            char letter = (char)('A' + (value - 1U));

            Serial.print("RX: ");
            Serial.println(letter);

            showLetter(letter);

            digitalWrite(LED_PIN, HIGH);
            delay(100U); 
            digitalWrite(LED_PIN, LOW);

            receivedCount++;
            lcd.setCursor(0, 0);
            lcd.print("CNT:");
            lcd.print(receivedCount);
            lcd.print("      "); 
        }

        mySwitch.resetAvailable();
    }
}

/*!
 * @brief    Wyswietla odebrana litere na dolnym wierszu wyswietlacza LCD i przesuwa kursor.
 * @param    letter
 * Znak do wyswietlenia na ekranie (od 'A' do 'Z').
 * @returns  brak
 * @side effects:
 * Aktualizuje globalna pozycje kursora (cursorPos), nadpisuje wiersz na wyswietlaczu, 
 * zeruje kursor i czysci dolna linie po jej zapelnieniu (czyli osiagnieciu LCD_COLS).
 */
static void showLetter(char letter)
{
    lcd.setCursor(cursorPos, 1);
    lcd.print(letter);

    cursorPos++;

    if (cursorPos >= LCD_COLS)
    {
        cursorPos = 0U;

        lcd.setCursor(0, 1);
        lcd.print("                ");
    }
}