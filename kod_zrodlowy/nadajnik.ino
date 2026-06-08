#include <Arduino.h>
#include <RCSwitch.h>

static const uint8_t BTN_MORSE = 4U;
static const uint8_t BTN_PLUS = 5U;
static const uint8_t BTN_MINUS = 3U;
static const uint8_t LED_PIN = 6U;
static const uint8_t BUZZER_PIN = 2U;
static const uint8_t LED_BLINK_PIN = 8U;

static RCSwitch mySwitch = RCSwitch();

static uint32_t pressStart = 0U;
static bool pressed = false;

static uint8_t morsePattern = 1U;
static uint8_t morseCount = 0U;

static uint32_t lastInputTime = 0U;
static uint16_t morseThreshold = 300U;
static uint32_t lastBlinkTime = 0U;
static bool blinkState = false;

static void handleSpeedButtons(void);
static char decodeMorse(uint8_t pattern);

/*!
 * @brief    Inicjalizuje piny wejścia/wyjścia, komunikację szeregową oraz moduł nadajnika.
 * @param    brak parametrow
 * brak
 * @returns  brak
 * @side effects:
 * Konfiguruje porty mikrokontrolera, inicjalizuje port szeregowy,
 * uruchamia nadajnik radiowy na zadanym pinie.
 */
void setup()
{
    pinMode(BTN_MORSE, INPUT_PULLUP);
    pinMode(BTN_PLUS, INPUT_PULLUP);
    pinMode(BTN_MINUS, INPUT_PULLUP);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_BLINK_PIN, OUTPUT);

    Serial.begin(9600);

    mySwitch.enableTransmit(10);

    Serial.println("Nadajnik gotowy");
}

/*!
 * @brief    Główna pętla programu. Obsługuje odczyt przycisków, miganie diody oraz dekodowanie i nadawanie znaków Morse'a.
 * @param    brak parametrow
 * brak
 * @returns  brak
 * @side effects:
 * Zmienia stany wyjść mikrokontrolera (diody, buzzer), modyfikuje zmienne globalne
 * dotyczące czasu i bufora, wysyła dane przez moduł radiowy oraz UART.
 */
void loop()
{
    uint32_t currentMillis = millis();

    handleSpeedButtons();

    if ((currentMillis - lastBlinkTime) >= morseThreshold)
    {
        lastBlinkTime = currentMillis;
        blinkState = !blinkState;
        digitalWrite(LED_BLINK_PIN, blinkState ? HIGH : LOW);
    }

    if (digitalRead(BTN_MORSE) == LOW)
    {
        digitalWrite(BUZZER_PIN, LOW);
    }
    else
    {
        digitalWrite(BUZZER_PIN, HIGH);
    }

    if ((digitalRead(BTN_MORSE) == LOW) && (!pressed))
    {
        pressStart = currentMillis;
        pressed = true;
    }

    if ((digitalRead(BTN_MORSE) == HIGH) && (pressed))
    {
        uint32_t duration = currentMillis - pressStart;
        pressed = false;

        if (morseCount < 6U)
        {
            if (duration < morseThreshold)
            {
                morsePattern = (uint8_t)((uint8_t)(morsePattern << 1U) | 0U);
                Serial.print(".");
            }
            else
            {
                morsePattern = (uint8_t)((uint8_t)(morsePattern << 1U) | 1U);
                Serial.print("-");
            }
            morseCount++;
        }

        lastInputTime = currentMillis;
    }

    if ((morseCount > 0U) && ((currentMillis - lastInputTime) > 800U) && (!pressed))
    {
        char letter = decodeMorse(morsePattern);

        if ((letter >= 'A') && (letter <= 'Z'))
        {
            uint32_t code = (uint32_t)(letter - 'A') + 1U;

            digitalWrite(LED_PIN, HIGH);
            mySwitch.send(code, 8);
            delay(100);
            digitalWrite(LED_PIN, LOW);

            Serial.print(" -> ");
            Serial.println(letter);
        }
        else
        {
            Serial.println("Nieznany kod");
        }

        morseCount = 0U;
        morsePattern = 1U;
    }
}

/*!
 * @brief    Odczytuje stan przycisków plus/minus i modyfikuje próg czasowy odróżniający kropkę od kreski.
 * @param    brak parametrow
 * brak
 * @returns  brak
 * @side effects:
 * Modyfikuje wartość globalnej zmiennej morseThreshold.
 * Wysyła aktualną wartość progu na port szeregowy.
 */
static void handleSpeedButtons(void)
{
    static uint32_t lastPress = 0U;
    uint32_t currentMillis = millis();

    if ((currentMillis - lastPress) >= 200U)
    {
        if (digitalRead(BTN_PLUS) == LOW)
        {
            if (morseThreshold < 1000U)
            {
                morseThreshold += 50U;
            }

            Serial.print("Prog: ");
            Serial.println(morseThreshold);

            lastPress = currentMillis;
        }
        else if (digitalRead(BTN_MINUS) == LOW)
        {
            if (morseThreshold > 100U)
            {
                morseThreshold -= 50U;
            }

            Serial.print("Prog: ");
            Serial.println(morseThreshold);

            lastPress = currentMillis;
        }
        else
        {
            /* Intentionally empty */
        }
    }
}

/*!
 * @brief    Dekoduje liczbowa reprezentacje bitowa znakow Morse'a na odpowiednia litere.
 * @param    pattern
 * Wartosc uint8_t zawierajaca zakodowana sekwencje kropek (0) i kresek (1).
 * @returns  Zdekodowany znak (litery 'A'-'Z') lub znak '?' w przypadku braku dopasowania.
 * @side effects:
 * brak
 */
static char decodeMorse(uint8_t pattern)
{
    char result;

    switch (pattern)
    {
        case 5U:   result = 'A'; break;
        case 24U:  result = 'B'; break;
        case 26U:  result = 'C'; break;
        case 12U:  result = 'D'; break;
        case 2U:   result = 'E'; break;
        case 18U:  result = 'F'; break;
        case 14U:  result = 'G'; break;
        case 16U:  result = 'H'; break;
        case 4U:   result = 'I'; break;
        case 23U:  result = 'J'; break;
        case 13U:  result = 'K'; break;
        case 20U:  result = 'L'; break;
        case 7U:   result = 'M'; break;
        case 6U:   result = 'N'; break;
        case 15U:  result = 'O'; break;
        case 22U:  result = 'P'; break;
        case 29U:  result = 'Q'; break;
        case 10U:  result = 'R'; break;
        case 8U:   result = 'S'; break;
        case 3U:   result = 'T'; break;
        case 9U:   result = 'U'; break;
        case 17U:  result = 'V'; break;
        case 11U:  result = 'W'; break;
        case 25U:  result = 'X'; break;
        case 27U:  result = 'Y'; break;
        case 28U:  result = 'Z'; break;
        default:   result = '?'; break;
    }

    return result;
}