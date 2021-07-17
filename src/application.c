#include <application.h>

#define BATTERY_UPDATE_INTERVAL (7 * 60 * 60 * 1000) // 7 hodin
#define BATTERY_UPDATE_SERVICE_INTERVAL (10 * 60 * 1000) // 10 minut
#define BATTERY_UPDATE_INITIAL_INTERVAL (60 * 1000) // 1 minuta

#define UPDATE_NORMAL_INTERVAL             (2000)

#define TEMPERATURE_TAG_PUB_VALUE_CHANGE 0.5f
#define HUMIDITY_TAG_PUB_VALUE_CHANGE 2

#define RADIO_SEND_INTERVAL 30 * 1000               // 30 sekund


struct {
    event_param_t temperature;
    event_param_t humidity;
    event_param_t illuminance;
    event_param_t pressure;
    event_param_t pressureMeters;

} params;

bool first_battery_send = true;


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

    (void) event;
    (void) event_param;

    float voltage;
    int percentage;

    if(event == TWR_MODULE_BATTERY_EVENT_UPDATE)
    {
        if (twr_module_battery_get_voltage(&voltage))
        {
            if(first_battery_send)
            {
                twr_module_battery_set_update_interval(BATTERY_UPDATE_SERVICE_INTERVAL);
                twr_scheduler_register(switch_to_normal_mode_task, NULL, SERVICE_INTERVAL_INTERVAL);
            }
            values.battery_voltage = voltage;
            twr_radio_pub_battery(&values.battery_voltage);
        }

        if (twr_module_battery_get_charge_level(&percentage))
        {
            values.battery_pct = percentage;
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
            if ((fabs(value - params.temperature.value) >= TEMPERATURE_TAG_PUB_VALUE_CHANGE))
            {
                twr_radio_pub_temperature(0, &value);
            }
            params.temperature.value = value;
            twr_scheduler_plan_now(0);
        }
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_HYGROMETER)
    {
        if (twr_module_climate_get_humidity_percentage(&value))
        {
            if ((fabs(value - params.humidity.value) >= HUMIDITY_TAG_PUB_VALUE_CHANGE))
            {
                twr_radio_pub_humidity(0, &value);
            }

            params.humidity.value = value;
            twr_scheduler_plan_now(0);
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
            if (((value == 0) && (params.illuminance.value != 0)) || ((value > 1) && (params.illuminance.value == 0)))
            {
                params.illuminance.value = value;
            }
        }
    }
    else if (event == TWR_MODULE_CLIMATE_EVENT_UPDATE_BAROMETER)
    {
        if (twr_module_climate_get_pressure_pascal(&value))
        {
            float meter;

            if (!twr_module_climate_get_altitude_meter(&meter))
            {
                return;
            }

            //twr_radio_pub_barometer(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &value, &meter);
            params.pressure.value = value;
            params.pressureMeters.value = meter;
        }
    }
}


void send_data_over_radio()
{
    twr_radio_pub_temperature(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &(params.temperature.value));
    twr_radio_pub_humidity(TWR_RADIO_PUB_CHANNEL_R3_I2C0_ADDRESS_DEFAULT, &(params.humidity.value));
    twr_radio_pub_luminosity(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &(params.illuminance.value));
    twr_radio_pub_barometer(TWR_RADIO_PUB_CHANNEL_R1_I2C0_ADDRESS_DEFAULT, &(params.pressure.value), &(params.pressureMeters.value));
    twr_scheduler_plan_current_from_now(RADIO_SEND_INTERVAL);

}

void switch_to_normal_mode_task(void *param)
{
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INTERVAL);
    twr_scheduler_unregister(twr_scheduler_get_current_task_id());
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
    twr_module_battery_set_update_interval(BATTERY_UPDATE_INITIAL_INTERVAL);

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
