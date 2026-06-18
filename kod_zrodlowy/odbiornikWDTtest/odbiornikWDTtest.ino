#include <Arduino.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>
#include <RCSwitch.h>
#include <stdlib.h>
#include <avr/interrupt.h>

/* =========================================================================
 * PROTOTYPY FUNKCJI (Zgodnosc z MISRA-C)
 * ========================================================================= */
void setup(void);
void loop(void);
static void showLetter(char letter);
void USART_init(uint16_t ubrr_value);
void USART_transmit(char data);
void USART_print(const char* str);
void USART_print_uint16(uint16_t num);
void EEPROM_write_byte(uint16_t address, uint8_t data);
uint8_t EEPROM_read_byte(uint16_t address);
void WDT_init(void);
void WDT_reset(void);

/* =========================================================================
 * STALE I ZMIENNE GLOBALNE
 * ========================================================================= */
static const uint8_t LED_PIN = 6U;
static const uint8_t BTN_CLEAR_PIN = 8U;

#define LCD_ADDR 0x27U
#define LCD_COLS 16U
#define LCD_ROWS 2U

static const uint16_t EEPROM_ADDR_CRASH_CNT = 0x0020U;
static const uint16_t EEPROM_ADDR_CRASH_FLAG = 0x0021U;

static LiquidCrystal_I2C lcd(LCD_ADDR, LCD_COLS, LCD_ROWS);
static RCSwitch mySwitch = RCSwitch();

static uint8_t cursorPos = 0U;

/* =========================================================================
 * IMPLEMENTACJA FUNKCJI
 * ========================================================================= */

/*!
 * @brief    Inicjalizuje sprzetowy interfejs USART0.
 * @param    ubrr_value Wartosc dzielnika czestotliwosci (Baud Rate)
 * @returns  Brak (void)
 * @side effects Modyfikuje rejestry UBRR0H, UBRR0L, UCSR0B, UCSR0C. Wlacza pin TXD.
 */
void USART_init(uint16_t ubrr_value)
{
    UBRR0H = (uint8_t)(ubrr_value >> 8U);
    UBRR0L = (uint8_t)(ubrr_value);
    UCSR0B = (1 << TXEN0);
    UCSR0C = (1 << UCSZ01) | (1 << UCSZ00);
}

/*!
 * @brief    Wysyla pojedynczy znak ASCII przez interfejs szeregowy.
 * @param    data Znak bajtowy do transmisji
 * @returns  Brak (void)
 * @side effects Oczekuje blokujaco (while) na flage UDRE0 (pusty bufor).
 */
void USART_transmit(char data)
{
    while ((UCSR0A & (1 << UDRE0)) == 0U) { }
    UDR0 = (uint8_t)data;
}

/*!
 * @brief    Wysyla ciag znakow (string) przez USART.
 * @param    str Wskaznik na tablice znakow zakonczona '\0'
 * @returns  Brak (void)
 * @side effects Kolejkuje znaki uzywajac USART_transmit, blokuje wykonanie na czas transmisji.
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
 * @brief    Wysyla liczbe calkowita (16-bit) przez USART w formie tekstu.
 * @param    num Liczba do konwersji i wyslania
 * @returns  Brak (void)
 * @side effects Zuzywa pamiec stosu na lokalny bufor konwersji.
 */
void USART_print_uint16(uint16_t num)
{
    char buffer[6];
    utoa(num, buffer, 10);
    USART_print(buffer);
}

/*!
 * @brief    Zapisuje pojedynczy bajt pod wskazany adres w sprzetowej pamieci EEPROM.
 * @param    address 10-bitowy adres komorki pamieci
 * @param    data 8-bitowa wartosc do zapisu
 * @returns  Brak (void)
 * @side effects Wykonuje atomowa operacje zapisu. Blokuje wykonanie na ~3.4 ms.
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
 * @brief    Odczytuje pojedynczy bajt z pamieci EEPROM.
 * @param    address 10-bitowy adres komorki pamieci
 * @returns  Odczytany bajt (uint8_t)
 * @side effects Czeka na zwolnienie magistrali zapisu przed wyzwoleniem odczytu.
 */
uint8_t EEPROM_read_byte(uint16_t address)
{
    while ((EECR & (1 << EEPE)) != 0U) { }
    EEAR = address;
    EECR |= (1 << EERE);
    return EEDR;
}

/*!
 * @brief    Inicjalizuje Watchdog Timer w trybie Interrupt & System Reset.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Zmienia rejestr WDTCSR.
 */
void WDT_init(void)
{
    cli(); 
    WDTCSR |= (uint8_t)((1U << WDCE) | (1U << WDE));
    WDTCSR = (uint8_t)((1U << WDIE) | (1U << WDE) | (1U << WDP2) | (1U << WDP1) | (1U << WDP0));
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
 * @brief    Wektor przerwania wykonywany ułamek sekundy przed resetem z powodu bledu WDT.
 * @param    Brak (Funkcja sprzetowa ISR)
 * @returns  Brak (void)
 * @side effects Nadpisuje dane awaryjne w EEPROM.
 */
// cppcheck-suppress misra-c2012-2.7
// cppcheck-suppress misra-c2012-8.2
ISR(WDT_vect)
{
    EEPROM_write_byte(EEPROM_ADDR_CRASH_FLAG, 1U);
    uint8_t crashCount = EEPROM_read_byte(EEPROM_ADDR_CRASH_CNT);
    if (crashCount == 0xFFU) { crashCount = 0U; }
    crashCount++;
    EEPROM_write_byte(EEPROM_ADDR_CRASH_CNT, crashCount);
}

/*!
 * @brief    Inicjalizuje system, peryferia, piny i diagnozuje awarie.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Zmienia stany poczatkowe wejsc/wyjsc, uruchamia I2C.
 */
void setup(void)
{
    USART_init(103U); // 9600 bodow

    uint8_t crashFlag = EEPROM_read_byte(EEPROM_ADDR_CRASH_FLAG);
    if (crashFlag == 1U)
    {
        uint8_t crashes = EEPROM_read_byte(EEPROM_ADDR_CRASH_CNT);
        USART_print("! KRYTYCZNY BLAD: Uklad zresetowany przez Watchdog !\r\n");
        USART_print("Liczba awarii w EEPROM: ");
        USART_print_uint16((uint16_t)crashes);
        USART_print("\r\n");
        EEPROM_write_byte(EEPROM_ADDR_CRASH_FLAG, 0U); 
    }
    else
    {
        USART_print("Normalny start systemu. Nasluch radiowy gotowy.\r\n");
    }

    pinMode(LED_PIN, OUTPUT);
    pinMode(BTN_CLEAR_PIN, INPUT_PULLUP);

    lcd.init();
    lcd.backlight();
    lcd.setCursor(0, 0);
    lcd.print("Odbiornik");

    mySwitch.enableReceive(0);
    WDT_init(); 
}

/*!
 * @brief    Glowna petla odbierajaca i wyswietlajaca dane.
 * @param    Brak
 * @returns  Brak (void)
 * @side effects Obsluguje logike radia i test zawieszenia systemu.
 */
void loop(void)
{
    static uint16_t receivedCount = 0U;

    WDT_reset(); 

    if (digitalRead(BTN_CLEAR_PIN) == LOW)
    {
        lcd.clear();
        lcd.setCursor(0, 0);
        lcd.print("Odbiornik");
        
        cursorPos = 0U;
        receivedCount = 0U;
        
        USART_print("Ekran wyczyszczony. TEST WATCHDOGA - ZAWIAS SYSTEMU...\r\n");
        while(1)
        {
            // Procesor utknął. ISR(WDT_vect) w drodze.
        }
    }

    if (mySwitch.available())
    {
        uint32_t value = (uint32_t)mySwitch.getReceivedValue();

        if ((value >= 1U) && (value <= 26U))
        {
            static const char alphabet[26] = {
                'A','B','C','D','E','F','G','H','I','J','K','L','M',
                'N','O','P','Q','R','S','T','U','V','W','X','Y','Z'
            };
            char letter = alphabet[value - 1U];

            USART_print("RX: ");
            USART_transmit(letter);
            USART_print("\r\n");

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
 * @brief    Wypisuje odkodowana litere na zdefiniowanej pozycji ekranu I2C.
 * @param    letter Znak ASCII do wyswietlenia
 * @returns  Brak (void)
 * @side effects Przesuwa kursor ekranu. Nadpisuje dolny wiersz ekranu po zapelnieniu.
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