#include <Arduino.h>
#include <RCSwitch.h>
#include <stdlib.h>
#include <avr/interrupt.h>

/* =========================================================================
 * PROTOTYPY FUNKCJI (Zgodnosc z MISRA-C)
 * ========================================================================= */
void setup(void);
void loop(void);
static void handleSpeedButtons(void);
static char decodeMorse(uint8_t pattern);
void USART_init(uint16_t ubrr_value);
void USART_transmit(char data);
void USART_print(const char* str);
void USART_print_uint16(uint16_t num);
void EEPROM_write_byte(uint16_t address, uint8_t data);
uint8_t EEPROM_read_byte(uint16_t address);
void EEPROM_write_uint16(uint16_t address, uint16_t data);
uint16_t EEPROM_read_uint16(uint16_t address);
void enterSleepMode(void);
void WDT_init(void);
void WDT_off(void);
void WDT_reset(void);

/* =========================================================================
 * STALE I ZMIENNE GLOBALNE
 * ========================================================================= */
static const uint8_t BTN_MORSE = 4U;
static const uint8_t BTN_PLUS = 5U;
static const uint8_t BTN_MINUS = 3U;
static const uint8_t BTN_RANDOM = 11U;
static const uint8_t LED_PIN = 6U;
static const uint8_t BUZZER_PIN = 2U;
static const uint8_t LED_BLINK_PIN = 8U;

static const uint16_t EEPROM_ADDR_THRESHOLD = 0x0010U;
static const uint16_t EEPROM_ADDR_CRASH_CNT = 0x0020U;
static const uint16_t EEPROM_ADDR_CRASH_FLAG = 0x0021U;

static RCSwitch mySwitch = RCSwitch();

static uint32_t lastActivityTime = 0U;
static uint32_t pressStart = 0U;
static bool pressed = false;
static bool randomPressed = false;

static uint8_t morsePattern = 1U;
static uint8_t morseCount = 0U;

static uint32_t lastInputTime = 0U;
static uint16_t morseThreshold = 300U; 
static uint32_t lastBlinkTime = 0U;
static bool blinkState = false;

/* =========================================================================
 * IMPLEMENTACJA FUNKCJI
 * ========================================================================= */

/*!
 * @brief    Wektor przerwania awaryjnego zglaszanego przez Watchdog Timer.
 * @param    Brak (Funkcja sprzetowa ISR)
 * @returns  Brak (void)
 * @side effects Zapisuje jedynke do flagi awarii i inkrementuje licznik WDT w EEPROM.
 */
ISR(WDT_vect)
{
    EEPROM_write_byte(EEPROM_ADDR_CRASH_FLAG, 1U);
    uint8_t crashCount = EEPROM_read_byte(EEPROM_ADDR_CRASH_CNT);
    if (crashCount == 0xFFU) { crashCount = 0U; }
    crashCount++;
    EEPROM_write_byte(EEPROM_ADDR_CRASH_CNT, crashCount);
}

/*!
 * @brief    Wektor wybudzenia asynchronicznego (PCINT) dla portu D.
 * @param    Brak (Funkcja sprzetowa ISR)
 * @returns  Brak (void)
 * @side effects Pusta funkcja uzywana wylacznie do wybudzenia rdzenia.
 */
ISR(PCINT2_vect) { }

/*!
 * @brief    Wektor wybudzenia asynchronicznego (PCINT) dla portu B.
 * @param    Brak (Funkcja sprzetowa ISR)
 * @returns  Brak (void)
 * @side effects Pusta funkcja uzywana wylacznie do wybudzenia rdzenia.
 */
ISR(PCINT0_vect) { }

/*!
 * @brief    Inicjalizuje Watchdog Timer w trybie Interrupt & System Reset na 2 sekundy.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Wylacza i wlacza globalne przerwania na czas sekwencji atomowej.
 */
void WDT_init(void)
{
    cli(); 
    WDTCSR |= (uint8_t)((1U << WDCE) | (1U << WDE));
    WDTCSR = (uint8_t)((1U << WDIE) | (1U << WDE) | (1U << WDP2) | (1U << WDP1) | (1U << WDP0));
    sei(); 
}

/*!
 * @brief    Zatrzymuje sprzętowy licznik Watchdoga przed usypianiem procesora.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Cysci flage WDRF oraz wylacza zasilanie modulu WDT.
 */
void WDT_off(void)
{
    cli();
    __asm__ __volatile__ ("wdr");
    MCUSR &= (uint8_t)(~(1U << WDRF));
    WDTCSR |= (uint8_t)((1U << WDCE) | (1U << WDE));
    WDTCSR = 0x00U;
    sei();
}

/*!
 * @brief    Resetuje licznik sprzetowy Watchdoga (karmi psa).
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Wykonuje bezposrednia wstawke asemblerowa 'wdr'.
 */
void WDT_reset(void)
{
    __asm__ __volatile__ ("wdr");
}

/*!
 * @brief    Przechodzi w tryb glebokiego uspenia sprzetowego (Power-Down).
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Wymusza gaszenie wyjsc, zatrzymuje WDT, modyfikuje SMCR, zatrzymuje zegar CPU.
 */
void enterSleepMode(void)
{
    USART_print("Brak aktywnosci. Usypianie systemu (Power-down)...\r\n");
    delay(50U); 

    // --- SPRZATANIE PRZED SNEM (Zwykly digitalWrite) ---
    digitalWrite(LED_BLINK_PIN, LOW);
    blinkState = false; 
    digitalWrite(LED_PIN, LOW);    
    digitalWrite(BUZZER_PIN, HIGH); 
    
    // Wylaczenie ADC
    ADCSRA &= (uint8_t)(~(1U << ADEN));
    
    // Aktywacja PCINT (PORTD i PORTB)
    PCICR |= (uint8_t)((1U << PCIE2) | (1U << PCIE0));
    PCMSK2 |= (uint8_t)((1U << PCINT19) | (1U << PCINT20) | (1U << PCINT21));
    PCMSK0 |= (uint8_t)(1U << PCINT3);

    // SMCR
    SMCR |= (uint8_t)(1U << SM1);
    SMCR &= (uint8_t)(~(1U << SM0));
    SMCR &= (uint8_t)(~(1U << SM2));
    SMCR |= (uint8_t)(1U << SE); // Sleep enable

    // WYLACZENIE WATCHDOGA NA CZAS SNU
    WDT_off();

    __asm__ __volatile__ ("sleep");

    // ==========================================
    // --- PROCESOR WYBUDZONY (Pobudka PCINT) ---
    // ==========================================
    
    SMCR &= (uint8_t)(~(1U << SE)); 
    PCICR &= (uint8_t)(~((1U << PCIE2) | (1U << PCIE0)));
    ADCSRA |= (uint8_t)(1U << ADEN);

    // WZNOWIENIE WATCHDOGA PO OBUDZENIU
    WDT_init();

    USART_print("Wznowienie pracy rdzenia!\r\n");
    lastActivityTime = millis();
}

/*!
 * @brief    Zapisuje pojedynczy bajt w pamieci EEPROM.
 * @param    address Adres komorki EEPROM
 * @param    data Bajt do zapisu
 * @returns  Brak (void)
 * @side effects Oczekuje w petli while na gotowosc zapisu.
 */
void EEPROM_write_byte(uint16_t address, uint8_t data)
{
    while ((EECR & (1 << EEPE)) != 0U) { }
    EEAR = address;
    EEDR = data;
    EECR |= (1 << EEMPE);
    EECR |= (1 << EEPE);
}

/*!
 * @brief    Odczytuje bajt danych z pamieci EEPROM.
 * @param    address Adres komorki do odczytu
 * @returns  Zwraca pojedynczy bajt (uint8_t)
 * @side effects Oczekuje blokujaco w petli while.
 */
uint8_t EEPROM_read_byte(uint16_t address)
{
    while ((EECR & (1 << EEPE)) != 0U) { }
    EEAR = address;
    EECR |= (1 << EERE);
    return EEDR;
}

/*!
 * @brief    Zapisuje wartosc 16-bitowa rozbita na dwie komorki EEPROM.
 * @param    address Poczatkowy adres rejestru
 * @param    data Wartosc 16-bitowa do zapisu
 * @returns  Brak (void)
 * @side effects Nadpisuje dwa sasiednie bajty (address oraz address+1).
 */
void EEPROM_write_uint16(uint16_t address, uint16_t data)
{
    uint8_t lowByte = (uint8_t)(data & 0xFFU);
    uint8_t highByte = (uint8_t)((data >> 8U) & 0xFFU);
    EEPROM_write_byte(address, lowByte);
    EEPROM_write_byte(address + 1U, highByte);
}

/*!
 * @brief    Odczytuje i scala wartosc 16-bitowa z dwoch sasiednich adresow EEPROM.
 * @param    address Poczatkowy adres odczytu
 * @returns  Zlozona wartosc (uint16_t)
 * @side effects Dokonuje operacji bitowego przesuniecia.
 */
uint16_t EEPROM_read_uint16(uint16_t address)
{
    uint8_t lowByte = EEPROM_read_byte(address);
    uint8_t highByte = EEPROM_read_byte(address + 1U);
    return (uint16_t)((uint16_t)lowByte | (uint16_t)((uint16_t)highByte << 8U));
}

/*!
 * @brief    Inicjalizuje blok asynchronicznej transmisji USART0.
 * @param    ubrr_value Dzielnik czestotliwosci procesora dla zadanej predkosci
 * @returns  Brak (void)
 * @side effects Przejmuje pin sprzętowy TXD.
 */
void USART_init(uint16_t ubrr_value)
{
    UBRR0H = (uint8_t)(ubrr_value >> 8U);
    UBRR0L = (uint8_t)(ubrr_value);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

/*!
 * @brief    Transmituje natychmiastowo jeden znak do bufora nadawczego.
 * @param    data 8-bitowy znak ASCII
 * @returns  Brak (void)
 * @side effects Wstrzymuje procesor do momentu opróżnienia rejestru przesuwnego (polling).
 */
void USART_transmit(char data)
{
    while ((UCSR0A & (1 << UDRE0)) == 0U) { }
    UDR0 = (uint8_t)data;
}

/*!
 * @brief    Iteruje i wysyla ciag tekstu.
 * @param    str Wskaznik bufora Stringa z terminatorem NULL
 * @returns  Brak (void)
 * @side effects Wolana wielokrotnie w pętli blokującej.
 */
void USART_print(const char* str)
{
    uint32_t i = 0U;
    while (str[i] != '\0')
    {
        USART_transmit(str[i]);
        i++;
    }
}

/*!
 * @brief    Zmienia liczbowa reprezentacje unsigned do stringu na cel wyslania na monitor szeregowy.
 * @param    num 16-bitowy integer
 * @returns  Brak (void)
 * @side effects Tworzy znakowa macierz w ramce stosu.
 */
void USART_print_uint16(uint16_t num)
{
    char buffer[6];
    utoa(num, buffer, 10);
    USART_print(buffer);
}

/*!
 * @brief    Boot systemu: Inicjuje piny, EEPROM, WDT i USART.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Zmienia tryby wejscia/wyjscia standardowym pinMode. Sprawdza flage awarii WDT.
 */
void setup(void)
{
    pinMode(BTN_MORSE, INPUT_PULLUP);
    pinMode(BTN_PLUS, INPUT_PULLUP);
    pinMode(BTN_MINUS, INPUT_PULLUP);
    pinMode(BTN_RANDOM, INPUT_PULLUP);

    pinMode(LED_PIN, OUTPUT);
    pinMode(BUZZER_PIN, OUTPUT);
    pinMode(LED_BLINK_PIN, OUTPUT);

    digitalWrite(BUZZER_PIN, HIGH); // Domyslne wylaczenie Active-Low buzzera

    USART_init(103U);
    mySwitch.enableTransmit(10U); 

    // ODCZYT FLAGI WATCHDOGA PO RESECIE
    uint8_t crashFlag = EEPROM_read_byte(EEPROM_ADDR_CRASH_FLAG);
    if (crashFlag == 1U)
    {
        uint8_t crashes = EEPROM_read_byte(EEPROM_ADDR_CRASH_CNT);
        USART_print("\r\n! KRYTYCZNY BLAD: Nadajnik zresetowany przez Watchdog !\r\n");
        USART_print("Liczba awarii w EEPROM: ");
        USART_print_uint16((uint16_t)crashes);
        USART_print("\r\n\r\n");
        EEPROM_write_byte(EEPROM_ADDR_CRASH_FLAG, 0U); 
    }
    else
    {
        USART_print("Normalny start systemu. Nadajnik gotowy.\r\n");
    }

    uint16_t savedThreshold = EEPROM_read_uint16(EEPROM_ADDR_THRESHOLD);
    if ((savedThreshold >= 100U) && (savedThreshold <= 1000U))
    {
        morseThreshold = savedThreshold;
        USART_print("Wczytano prog z EEPROM: ");
    }
    else
    {
        EEPROM_write_uint16(EEPROM_ADDR_THRESHOLD, morseThreshold);
        USART_print("Czysty EEPROM. Zapisano domyslny prog: ");
    }
    USART_print_uint16(morseThreshold);
    USART_print(" ms\r\n");

    WDT_init(); // Zabezpieczenie pętli glownej
}

/*!
 * @brief    Glowna rutyna obslugujaca czas, przyciski, dzwieki i wchodzenie w stan uspienia.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Analizuje Jitter czlowieka, posiada filtr drgan i resetuje sensory bezczynnosci.
 */
void loop(void)
{
    WDT_reset(); // KARMIENIE PSA W KAZDYM CYKLU PETLI
    
    uint32_t currentMillis = millis();

    handleSpeedButtons();

    // Miganie diodą synchronizacji
    if ((currentMillis - lastBlinkTime) >= morseThreshold)
    {
        lastBlinkTime = currentMillis;
        blinkState = !blinkState;
        digitalWrite(LED_BLINK_PIN, blinkState ? HIGH : LOW);
    }

    // Obsluga buzzera i resetowanie czasu bezczynnosci
    if (digitalRead(BTN_MORSE) == LOW)
    {
        digitalWrite(BUZZER_PIN, LOW); // Włączenie buzzera
        lastActivityTime = currentMillis; 
    }
    else
    {
        digitalWrite(BUZZER_PIN, HIGH); // Wyłączenie buzzera
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

        // Sprzętowy Debouncing (eliminacja drgań styków - min. 30ms)
        if (duration > 30U) 
        {
            if (morseCount < 6U)
            {
                if (duration < morseThreshold)
                {
                    morsePattern = (uint8_t)((uint8_t)(morsePattern << 1U) | 0U);
                    USART_transmit('.');
                }
                else
                {
                    morsePattern = (uint8_t)((uint8_t)(morsePattern << 1U) | 1U);
                    USART_transmit('-');
                }
                morseCount++;
            }
            lastInputTime = currentMillis;
        }
    }

    if ((morseCount > 0U) && ((currentMillis - lastInputTime) > 800U) && (!pressed))
    {
        char letter = decodeMorse(morsePattern);

        if ((letter >= 'A') && (letter <= 'Z'))
        {
            uint32_t code = (uint32_t)(letter - 'A') + 1U;

            digitalWrite(LED_PIN, HIGH);
            mySwitch.send(code, 8);
            delay(100U);
            digitalWrite(LED_PIN, LOW);

            USART_print(" -> ");
            USART_transmit(letter);
            USART_print("\r\n");
        }
        else
        {
            USART_print("Nieznany kod\r\n");
        }

        morseCount = 0U;
        morsePattern = 1U;
    }

    // Odczyt TRNG przyciskiem RANDOM
    if (digitalRead(BTN_RANDOM) == LOW)
    {
        lastActivityTime = currentMillis; 

        if (!randomPressed)
        {
            randomPressed = true;
            uint8_t hardwareRandomValue = TCNT0; 
            USART_print("Wygenerowana losowa entropia: ");
            USART_print_uint16((uint16_t)hardwareRandomValue);
            USART_print("\r\n");
            
            uint8_t offset = (uint8_t)(hardwareRandomValue % 26U);
            char randomLetter = (char)('A' + offset);

            USART_print("Wylosowano znak: ");
            USART_transmit(randomLetter);
            USART_print("\r\n");

            uint32_t code = (uint32_t)(randomLetter - 'A') + 1U;

            digitalWrite(LED_PIN, HIGH);
            mySwitch.send(code, 8U);
            delay(100U);
            digitalWrite(LED_PIN, LOW);
        }
    }
    else 
    {
        randomPressed = false;
    }
    
    // Usypianie systemu (20 sekund bezczynnosci)
    if ((currentMillis - lastActivityTime) > 20000U)
    {
        enterSleepMode();
    }
}

/*!
 * @brief    Monitoruje stany przyciskow predkosci do zmiany progu Morse'a z zapisem.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Zrzuca ustalona wartosc trwale do komorki EEPROM.
 */
static void handleSpeedButtons(void)
{
    static uint32_t lastPress = 0U;
    uint32_t currentMillis = millis();

    if ((currentMillis - lastPress) >= 200U)
    {
        if (digitalRead(BTN_PLUS) == LOW)
        {
            lastActivityTime = currentMillis;

            if (morseThreshold < 1000U)
            {
                morseThreshold += 50U;
                EEPROM_write_uint16(EEPROM_ADDR_THRESHOLD, morseThreshold);
            }
            USART_print("Prog: ");
            USART_print_uint16(morseThreshold);
            USART_print("\r\n");
            lastPress = currentMillis;
        }
        else if (digitalRead(BTN_MINUS) == LOW)
        {
            lastActivityTime = currentMillis;

            if (morseThreshold > 100U)
            {
                morseThreshold -= 50U;
                EEPROM_write_uint16(EEPROM_ADDR_THRESHOLD, morseThreshold);
            }
            USART_print("Prog: ");
            USART_print_uint16(morseThreshold);
            USART_print("\r\n");
            lastPress = currentMillis;
        }
        else
        {
            /* Intentionally empty wg MISRA-C */
        }
    }
}

/*!
 * @brief    Konwertuje wzor bitowy na odpowiednia litere z dekady Morse'a.
 * @param    pattern Zlozony wzor binarny z kropek(0) i kresek(1).
 * @returns  Znak ASCII reprezentujacy litere (char).
 * @side effects Zwraca znak zapytania jezeli uklad jest nierozpoznany.
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