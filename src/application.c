#include <application.h>

#define BATTERY_UPDATE_INTERVAL   (2000)

#define UPDATE_NORMAL_INTERVAL             (3500)
#define BAROMETER_UPDATE_NORMAL_INTERVAL   (3500)

#define TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL (3500)

#define HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL (3500)

#define LUX_METER_TAG_PUB_NO_CHANGE_INTEVAL (3500)

#define BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL (3500)

struct {
    event_param_t temperature;
    event_param_t humidity;
    event_param_t illuminance;
    event_param_t pressure;
    event_param_t pressureMeters;

} params;

// LED instance
twr_led_t led;

// Thermometer instance
twr_tmp112_t tmp112;

// Button instance
twr_button_t button;
uint16_t button_event_count = 0;

void button_event_handler(twr_button_t *self, twr_button_event_t event, void *event_param)
{
    (void) self;
    (void) event_param;

    if (event == TWR_BUTTON_EVENT_PRESS)
    {
        twr_led_pulse(&led, 100);
    }
}

void battery_event_handler(twr_module_battery_event_t event, void *event_param)
{
    (void) event_param;

    float voltage;

    if (event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            twr_radio_pub_battery(&voltage);
            twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &(params.temperature.value));
            twr_radio_pub_humidity(TWR_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT, &(params.humidity.value));
            twr_radio_pub_luminosity(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &(params.illuminance.value));
            twr_radio_pub_barometer(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &(params.pressure.value), &(params.pressureMeters.value));
        }
    }
}


void climate_module_event_handler(twr_module_climate_event_t event, void *event_param)
{
    (void) event_param;

    float value;

    if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_THERMOMETER)
    {
        if (twr_module_climate_get_temperature_celsius(&value))
        {
            if ((params.temperature.next_pub < twr_scheduler_get_spin_tick()))
            {
                //twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &value);
                params.temperature.value = value;
                params.temperature.next_pub = twr_scheduler_get_spin_tick() + TEMPERATURE_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_HYGROMETER)
    {
        if (twr_module_climate_get_humidity_percentage(&value))
        {
            if ((params.humidity.next_pub < twr_scheduler_get_spin_tick()))
            {
                //twr_radio_pub_humidity(TWR_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT, &value);
                params.humidity.value = value;
                params.humidity.next_pub = twr_scheduler_get_spin_tick() + HUMIDITY_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_LUX_METER)
    {
        if (twr_module_climate_get_illuminance_lux(&value))
        {
            if (value < 1)
            {
                value = 0;
            }
            if ((params.illuminance.next_pub < twr_scheduler_get_spin_tick()) ||
                    ((value == 0) && (params.illuminance.value != 0)) || ((value > 1) && (params.illuminance.value == 0)))
            {
                //twr_radio_pub_luminosity(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &value);
                params.illuminance.value = value;
                params.illuminance.next_pub = twr_scheduler_get_spin_tick() + LUX_METER_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_BAROMETER)
    {
        if (twr_module_climate_get_pressure_pascal(&value))
        {
            if ((params.pressure.next_pub < twr_scheduler_get_spin_tick()))
            {
                float meter;

                if (!twr_module_climate_get_altitude_meter(&meter))
                {
                    return;
                }

                //twr_radio_pub_barometer(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &value, &meter);
                params.pressure.value = value;
                params.pressureMeters.value = meter;

                params.pressure.next_pub = twr_scheduler_get_spin_tick() + BAROMETER_TAG_PUB_NO_CHANGE_INTEVAL;
            }
        }
    }
}

void application_init(void)
{
    // Initialize LED
    twr_led_init(&led, TWR_GPIO_LED, false, false);
    twr_led_set_mode(&led, TWR_LED_MODE_OFF);

    // Initialize thermometer sensor on core module
    twr_tmp112_init(&tmp112, TWR_I2C_I2C0, 0x49);

    // Initialize radio
    twr_radio_init(TWR_RADIO_MODE_NODE_SLEEPING);

    // Initialize button
    twr_button_init(&button, TWR_GPIO_BUTTON, TWR_GPIO_PULL_DOWN, false);
    twr_button_set_event_handler(&button, button_event_handler, &button_event_count);

    // Initialize battery
    twr_module_battery_init();
    twr_module_battery_set_event_handler(battery_event_handler, NULL);
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);

    // Initialize climate module
    twr_module_climate_init();
    twr_module_climate_set_event_handler(climate_module_event_handler, NULL);
    twr_module_climate_set_update_interval_thermometer(UPDATE_NORMAL_INTERVAL);
    twr_module_climate_set_update_interval_hygrometer(UPDATE_NORMAL_INTERVAL);
    twr_module_climate_set_update_interval_lux_meter(UPDATE_NORMAL_INTERVAL);
    twr_module_climate_set_update_interval_barometer(UPDATE_NORMAL_INTERVAL);
    twr_module_climate_measure_all_sensors();

    twr_radio_pairing_request("climate-monitor", VERSION);

    twr_led_pulse(&led, 2000);
}
