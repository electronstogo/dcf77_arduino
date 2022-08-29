#ifndef DCF77_H_
#define DCF77_H_

#include <Arduino.h>

#define DCF77_DATA_PIN		2
#define DCF77_ENABLE_PIN	4



class DCF77
{
    private:
        void calculate_time_from_dcf77_data(uint8_t* dcf_data, uint32_t last_signal_timestamp);
        void handle_dcf_signal(int16_t signal_low_time, uint16_t* dcf_data_index, uint8_t* dcf_data);
        uint8_t get_bit_value(uint8_t *data, uint16_t index);

    public:
        DCF77();
        bool syncronize_time();
        uint8_t minutes;
        uint8_t hours;
        uint8_t week_day;
        uint8_t day_of_month;
        uint8_t month;
        uint8_t year;
};




#endif
