#include "sense.h"
#include "matter_air_quality_sensor.h"

float AirQualitySensor::getTemperature() { return (float)TEMP; }
float AirQualitySensor::getHumidity() { return (float)RH; }     
float AirQualitySensor::getCO2() { return (float)CO2; }

using namespace esp_matter;
using namespace esp_matter::endpoint;
using namespace chip::app::Clusters;
using namespace esp_matter::cluster; 

MatterAirQualitySensor::MatterAirQualitySensor(endpoint_t* endpoint, std::shared_ptr<AirQualitySensor> airQualitySensor)
    : m_endpoint(endpoint), m_airQualitySensor(airQualitySensor) {}

std::shared_ptr<MatterAirQualitySensor> MatterAirQualitySensor::CreateEndpoint(
    node_t* node, 
    std::shared_ptr<AirQualitySensor> airQualitySensor) 
{
    // 1. Create the base Air Quality Endpoint
    air_quality_sensor::config_t air_quality_config;
    endpoint_t* endpoint = air_quality_sensor::create(node, &air_quality_config, ENDPOINT_FLAG_NONE, nullptr);

    if (!endpoint) {
        log_e("Failed to create Air Quality endpoint!");
        return nullptr;
    }

    // 2. Add "Device Types" 
    // This tells HomeKit to treat this single endpoint as multiple virtual devices.
    // 0x0302 = Temperature Sensor
    // 0x0307 = Humidity Sensor
    endpoint::add_device_type(endpoint, 0x0302, 1); 
    endpoint::add_device_type(endpoint, 0x0307, 1);

    // -------------------------------------------------------------------------
    // 3. Create Clusters with Mandatory Attributes for HomeKit
    // -------------------------------------------------------------------------

    // --- Temperature ---
    temperature_measurement::config_t temp_config;
    cluster_t *temp_cluster = temperature_measurement::create(endpoint, &temp_config, CLUSTER_FLAG_SERVER);
    
    // Explicitly add Min and Max (Required by HomeKit to display the dial/number)
    // Values are in 0.01 degrees C. (-40C to 85C)
    attribute::create(temp_cluster, TemperatureMeasurement::Attributes::MinMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_int16(-4000));
    attribute::create(temp_cluster, TemperatureMeasurement::Attributes::MaxMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_int16(8500));
    // Ensure MeasuredValue exists
    attribute::create(temp_cluster, TemperatureMeasurement::Attributes::MeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_int16(0));

    // --- Humidity ---
    relative_humidity_measurement::config_t humid_config;
    cluster_t *humid_cluster = relative_humidity_measurement::create(endpoint, &humid_config, CLUSTER_FLAG_SERVER);
    attribute::create(humid_cluster, RelativeHumidityMeasurement::Attributes::MinMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint16(0));
    attribute::create(humid_cluster, RelativeHumidityMeasurement::Attributes::MaxMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint16(10000));
    attribute::create(humid_cluster, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint16(0));

    // --- CO2 ---
    carbon_dioxide_concentration_measurement::config_t co2_config;
    cluster_t *co2_cluster = carbon_dioxide_concentration_measurement::create(endpoint, &co2_config, CLUSTER_FLAG_SERVER);
    // Create MeasuredValue (Single Precision Float)
    attribute::create(co2_cluster, CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(0.0));

    return std::make_shared<MatterAirQualitySensor>(endpoint, airQualitySensor);
}

void MatterAirQualitySensor::UpdateMeasurements() {
    if (!m_airQualitySensor) return;

    // --- Temperature (0.01 C) ---
    float temperature = m_airQualitySensor->getTemperature();
    int16_t temp_matter = static_cast<int16_t>(temperature * 100);
    UpdateAttribute(TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, temp_matter);

    // --- Humidity (0.01 %) ---
    float humidity = m_airQualitySensor->getHumidity();
    uint16_t hum_matter = static_cast<uint16_t>(humidity * 100);
    UpdateAttribute(RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, hum_matter);

    // --- CO2 (Float PPM) ---
    float co2 = m_airQualitySensor->getCO2();
    UpdateAttribute(CarbonDioxideConcentrationMeasurement::Id, CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, co2);

    // --- Air Quality Logic ---
    using AirQualityEnum = AirQuality::AirQualityEnum;
    AirQualityEnum overallQuality = AirQualityEnum::kGood;

    if (co2 > 1000) overallQuality = AirQualityEnum::kFair;
    if (co2 > 1500) overallQuality = AirQualityEnum::kPoor;

    // Cast to uint8_t or int16_t depending on how the Enum is defined in your specific SDK version
    UpdateAttribute(AirQuality::Id, AirQuality::Attributes::AirQuality::Id, static_cast<int16_t>(overallQuality));
}

// --- Helper Implementations ---

// FIX 3: Use esp_matter::endpoint::get_id() instead of m_endpoint->get_endpoint_id()
// m_endpoint is a C-struct/int pointer, not a C++ class with methods.

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, float value) {
    esp_matter_attr_val_t val = esp_matter_float(value);
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
}

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, uint16_t value) {
    esp_matter_attr_val_t val = esp_matter_uint16(value);
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
}

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, int16_t value) {
    esp_matter_attr_val_t val = esp_matter_int16(value);
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
}