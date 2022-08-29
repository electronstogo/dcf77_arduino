#include "dcf77.h"
#include "Arduino.h"



DCF77::DCF77()
{
    // Configurate the DCF77 pins.
    pinMode(DCF77_DATA_PIN, INPUT);
    pinMode(DCF77_ENABLE_PIN, OUTPUT);

    // Activate the pull up. DCF77 signal comes from a open collector output.
    digitalWrite(DCF77_DATA_PIN, HIGH);

    // Activate the DCF77 module.
    digitalWrite(DCF77_ENABLE_PIN, HIGH);
    delay(100);
    digitalWrite(DCF77_ENABLE_PIN, LOW);
}



// Values for interrupt signal handling.
uint32_t timestamp_high;
uint32_t timestamp_low;
bool dcf_signal_triggered = false;


// External interrupt triggered by DCF signals.
ISR(INT0_vect)
{
    // Variables to keep last signal edge and timestamp.
    static uint8_t last_signal = 1;
    uint32_t current_timestamp = millis();


    // Check signal level, and keep timestamp.
    if(digitalRead(DCF77_DATA_PIN) && !last_signal)
    {
        last_signal = 1;
        timestamp_high = current_timestamp;
        dcf_signal_triggered = true;
    }
    else if(!digitalRead(DCF77_DATA_PIN) && last_signal)
    {
        last_signal = 0;
        timestamp_low = current_timestamp;
    }
}


// Function trys to receive the current time from DCF77.
// A watchdog timer cancels the function, if the process takes too long.
bool DCF77::syncronize_time()
{
    // Enable external interrupt for both edges at INT0 pin.
    EICRA |= 0x01;
    EIMSK |= 0x01;


    // array to keep the received data.
    uint8_t dcf_data[8];
    memset(dcf_data, 0, 8);

    uint16_t dcf_data_index = 1234;

    // Watchdog to prevent endless loop.
    uint16_t watchdog = 0;

    // Timestamp for calculation of waiting time.
    uint32_t signal_timestamp;

    while(true)
    {
        // Wait for next DCF77 signal.
        while(!dcf_signal_triggered)
        {
            delay(1);
        }

        signal_timestamp = millis();

        // Analyze the signal timings.
        this->handle_dcf_signal(timestamp_high - timestamp_low, &dcf_data_index, dcf_data);

        watchdog += 1;
        dcf_signal_triggered = false;



        // Cancel the process in case of bad signal.
        if(watchdog > 300)
        {
            // Disable extern Interrupt for DCF77.
            EIMSK &= ~0x01;
            return false;
        }

        // Transmission finished.
        if(dcf_data_index == 58)
        {
            calculate_time_from_dcf77_data(dcf_data, signal_timestamp);

            // Disable extern Interrupt for DCF77.
            EIMSK &= ~0x01;

            // Validate the received time data.
            bool result = this->minutes < 60;
            result = (this->hours < 24) && result;
            result = (this->week_day <= 7) && result;
            result = (this->day_of_month <= 31) && result;
            result = (this->month <= 12) && result;
            result = (this->year < 100) && result;

            return result;
        }
    }
}



// Function analyzes the signal timings, and updates the DCF77 time data.
void DCF77::handle_dcf_signal(int16_t signal_low_time, uint16_t* dcf_data_index, uint8_t* dcf_data)
{
    // Transmission start signal.
    if(signal_low_time > 1500)
    {
        *dcf_data_index = 0;
        return;
    }

    // catch invalid array indices.
    if(*dcf_data_index >= 60)
    {
        return;
    }


    // set data from timing interpretation.
    if(signal_low_time >= 750 && signal_low_time < 850)
    {
        dcf_data[*dcf_data_index / 8] |= (0x1 << (*dcf_data_index % 8));
    }
    else if(signal_low_time >= 850 && signal_low_time < 950)
    {
        // 0 is already set.
    }
    else
    {
        // Catch error timings.
        *dcf_data_index = 1234;
        return;
    }

    (*dcf_data_index)++;
}



// Calculate the time components from DCF77 data.
void DCF77::calculate_time_from_dcf77_data(uint8_t* dcf_data, uint32_t last_signal_timestamp)
{
    const uint16_t factors[8] = {1, 2, 4, 8, 10, 20, 40};


    // Wait until the :00 seconds moment after transmission.
    int16_t waiting_time = 2000 - (millis() - last_signal_timestamp) - 250;

    if(waiting_time > 0)
    {
        delay(waiting_time);
    }

    this->minutes = 0;
    this->hours = 0;
    this->day_of_month = 0;
    this->week_day = 0;
    this->month = 0;
    this->year = 0;

    for(uint16_t i = 0; i < 7; i++)
    {
        this->minutes += factors[i] * get_bit_value(dcf_data, 21 + i);
        this->year += factors[i] * get_bit_value(dcf_data, 50 + i);

        if(i >= 6)
        {
            continue;
        }

        this->hours += factors[i] * get_bit_value(dcf_data, 29 + i);
        this->day_of_month += factors[i] * get_bit_value(dcf_data, 36 + i);

        if(i >= 5)
        {
            continue;
        }

        this->month += factors[i] * get_bit_value(dcf_data, 45 + i);

        if(i >= 3)
        {
            continue;
        }

        this->week_day += factors[i] * get_bit_value(dcf_data, 42 + i);
    }
}



// Get bit value from data by index.
uint8_t DCF77::get_bit_value(uint8_t *data, uint16_t index)
{
    return (data[index / 8] & (0x1 << (index % 8)) ? 1 : 0);
}
