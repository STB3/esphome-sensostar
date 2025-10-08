import esphome.codegen as cg
import esphome.config_validation as cv
from esphome.components import sensor
from esphome.const import (
    CONF_ENERGY,
    CONF_VOLUME,
    
    CONF_POWER,
    CONF_FLOW,
    
    DEVICE_CLASS_ENERGY,
    DEVICE_CLASS_VOLUME,
    DEVICE_CLASS_POWER,
    DEVICE_CLASS_VOLUME_FLOW_RATE,
    DEVICE_CLASS_TEMPERATURE,
    
    UNIT_KILOWATT_HOURS,
    UNIT_CUBIC_METER,
    UNIT_WATT,
    UNIT_CUBIC_METER_PER_HOUR,
    UNIT_CELSIUS,
    
    ICON_POWER,
    ICON_THERMOMETER,
    
    STATE_CLASS_MEASUREMENT,
    STATE_CLASS_TOTAL_INCREASING,

)
from . import SensoStarComponent, CONF_SENSOSTAR_ID

CONF_TEMPERATURE_FLOW = "temperature_flow"
CONF_TEMPERATURE_RETURN = "temperature_return"
CONF_TEMPERATURE_DIFF = "temperature_diff"
CONF_CALCULATED_POWER = "calculated_power"

TYPES = [
    CONF_ENERGY,
    CONF_VOLUME,
    CONF_POWER,
    CONF_FLOW,
    CONF_TEMPERATURE_FLOW,
    CONF_TEMPERATURE_RETURN,
    CONF_TEMPERATURE_DIFF,
    CONF_CALCULATED_POWER
]

CONFIG_SCHEMA = cv.All(
    cv.Schema(
        {
            cv.GenerateID(CONF_SENSOSTAR_ID): cv.use_id(SensoStarComponent),
            cv.Optional(CONF_ENERGY): sensor.sensor_schema(
                unit_of_measurement=UNIT_KILOWATT_HOURS,
                icon=ICON_POWER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_ENERGY,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_VOLUME): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLUME,
                state_class=STATE_CLASS_TOTAL_INCREASING,
            ),
            cv.Optional(CONF_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_CUBIC_METER_PER_HOUR,
                accuracy_decimals=3,
                device_class=DEVICE_CLASS_VOLUME_FLOW_RATE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_FLOW): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_RETURN): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_TEMPERATURE_DIFF): sensor.sensor_schema(
                unit_of_measurement=UNIT_CELSIUS,
                icon=ICON_THERMOMETER,
                accuracy_decimals=2,
                device_class=DEVICE_CLASS_TEMPERATURE,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
            cv.Optional(CONF_CALCULATED_POWER): sensor.sensor_schema(
                unit_of_measurement=UNIT_WATT,
                icon=ICON_POWER,
                accuracy_decimals=0,
                device_class=DEVICE_CLASS_POWER,
                state_class=STATE_CLASS_MEASUREMENT,
            ),
        }
    ).extend(cv.COMPONENT_SCHEMA)
)

async def setup_conf(config, key, hub):
    if sensor_config := config.get(key):
        sens = await sensor.new_sensor(sensor_config)
        cg.add(getattr(hub, f"set_{key}_sensor")(sens))


async def to_code(config):
    hub = await cg.get_variable(config[CONF_SENSOSTAR_ID])
    for key in TYPES:
        await setup_conf(config, key, hub)
