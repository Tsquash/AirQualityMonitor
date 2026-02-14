#include "sense.h"
#include "matter_air_quality_sensor.h"
#include <platform/CHIPDeviceLayer.h>

float AirQualitySensor::getTemperature() { return tempC; }
float AirQualitySensor::getHumidity() { return (float)RH; }     
float AirQualitySensor::getCO2() { return (float)CO2; }
int32_t AirQualitySensor::getVOCIndex() { return VOC; }
int32_t AirQualitySensor::getNOxIndex() { return NOx; }

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

    // 2. Add Device Types
    endpoint::add_device_type(endpoint, 0x0302, 1); // Temperature Sensor
    endpoint::add_device_type(endpoint, 0x0307, 1); // Humidity Sensor

    // -------------------------------------------------------------------------
    // TEMPERATURE
    // -------------------------------------------------------------------------
    temperature_measurement::config_t temp_config;
    cluster_t *temp_cluster = temperature_measurement::create(endpoint, &temp_config, CLUSTER_FLAG_SERVER);
    
    // Min/Max/MeasuredValue
    attribute::create(temp_cluster, TemperatureMeasurement::Attributes::MinMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_int16(-4000));
    attribute::create(temp_cluster, TemperatureMeasurement::Attributes::MaxMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_int16(8500));
    attribute::create(temp_cluster, TemperatureMeasurement::Attributes::MeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_int16(0));

    // -------------------------------------------------------------------------
    // HUMIDITY
    // -------------------------------------------------------------------------
    relative_humidity_measurement::config_t humid_config;
    cluster_t *humid_cluster = relative_humidity_measurement::create(endpoint, &humid_config, CLUSTER_FLAG_SERVER);
    
    // Min/Max/MeasuredValue
    attribute::create(humid_cluster, RelativeHumidityMeasurement::Attributes::MinMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint16(0));
    attribute::create(humid_cluster, RelativeHumidityMeasurement::Attributes::MaxMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint16(10000));
    attribute::create(humid_cluster, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint16(0));

    // -------------------------------------------------------------------------
    // CO2 (Concentration Measurement)
    // -------------------------------------------------------------------------
    carbon_dioxide_concentration_measurement::config_t co2_config;
    // REMOVED: co2_config.feature_map = 0x0F; (This caused the error)

    cluster_t *co2_cluster = carbon_dioxide_concentration_measurement::create(endpoint, &co2_config, CLUSTER_FLAG_SERVER);
    
    // FIX: Manually update the Feature Map Attribute (ID 0xFFFC)
    // We want to enable NumericMeasurement (Bit 0), LevelIndication (Bit 1), Medium (Bit 2), Critical (Bit 3) -> 0x0F
    // We get the attribute pointer and set the value directly.
    attribute_t *featureMapAttr = attribute::get(co2_cluster, 0xFFFC); // 0xFFFC is the standard FeatureMap ID
    if (featureMapAttr) {
        esp_matter_attr_val_t val = esp_matter_uint32(0x0F);
        attribute::set_val(featureMapAttr, &val);
    }

    // Mandatory Attributes for Numeric Mode:
    attribute::create(co2_cluster, CarbonDioxideConcentrationMeasurement::Attributes::MinMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(0.0));
    attribute::create(co2_cluster, CarbonDioxideConcentrationMeasurement::Attributes::MaxMeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(5000.0));
    attribute::create(co2_cluster, CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(0.0));

    // Measurement Unit (0x0004) -> PPM (0)
    attribute::create(co2_cluster, CarbonDioxideConcentrationMeasurement::Attributes::MeasurementUnit::Id, ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint8(0));

    // Add VOC and NOx clusters to the Matter endpoint
    // -------------------------------------------------------------------------
    // VOC (tVOC Measurement)
    // -------------------------------------------------------------------------
    total_volatile_organic_compounds_concentration_measurement::config_t voc_config;
    cluster_t *voc_cluster = total_volatile_organic_compounds_concentration_measurement::create(
        endpoint, &voc_config, CLUSTER_FLAG_SERVER);

    attribute_t *vocFeatureMapAttr = attribute::get(voc_cluster, 0xFFFC);
    if (vocFeatureMapAttr) {
        esp_matter_attr_val_t val = esp_matter_uint32(0x0F);
        attribute::set_val(vocFeatureMapAttr, &val);
    }

    attribute::create(voc_cluster, TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MinMeasuredValue::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(1.0));
    attribute::create(voc_cluster, TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MaxMeasuredValue::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(500.0));
    attribute::create(voc_cluster, TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasuredValue::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(1.0));
    attribute::create(voc_cluster, TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasurementUnit::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint8(4)); // ug/m3
    attribute::create(voc_cluster, TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasurementMedium::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint8(0)); // air

    // -------------------------------------------------------------------------
    // NOx (NO2 Measurement)
    // -------------------------------------------------------------------------
    nitrogen_dioxide_concentration_measurement::config_t nox_config;
    cluster_t *nox_cluster = nitrogen_dioxide_concentration_measurement::create(
        endpoint, &nox_config, CLUSTER_FLAG_SERVER);

    attribute_t *noxFeatureMapAttr = attribute::get(nox_cluster, 0xFFFC);
    if (noxFeatureMapAttr) {
        esp_matter_attr_val_t val = esp_matter_uint32(0x0F);
        attribute::set_val(noxFeatureMapAttr, &val);
    }

    attribute::create(nox_cluster, NitrogenDioxideConcentrationMeasurement::Attributes::MinMeasuredValue::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(1.0));
    attribute::create(nox_cluster, NitrogenDioxideConcentrationMeasurement::Attributes::MaxMeasuredValue::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(500.0));
    attribute::create(nox_cluster, NitrogenDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_float(1.0));
    attribute::create(nox_cluster, NitrogenDioxideConcentrationMeasurement::Attributes::MeasurementUnit::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint8(4)); // ug/m3 (wrong unit :( only one that works)
    attribute::create(nox_cluster, NitrogenDioxideConcentrationMeasurement::Attributes::MeasurementMedium::Id,
                      ATTRIBUTE_FLAG_NULLABLE, esp_matter_uint8(0)); // air

    return std::make_shared<MatterAirQualitySensor>(endpoint, airQualitySensor);
}

void MatterAirQualitySensor::UpdateMeasurements() {
    if (!m_airQualitySensor) return;

    // --- Temperature ---
    float temperature = m_airQualitySensor->getTemperature();
    int16_t temp_matter = static_cast<int16_t>(temperature * 100);
    UpdateAttribute(TemperatureMeasurement::Id, TemperatureMeasurement::Attributes::MeasuredValue::Id, temp_matter);

    // --- Humidity ---
    float humidity = m_airQualitySensor->getHumidity();
    uint16_t hum_matter = static_cast<uint16_t>(humidity * 100);
    UpdateAttribute(RelativeHumidityMeasurement::Id, RelativeHumidityMeasurement::Attributes::MeasuredValue::Id, hum_matter);

    // --- CO2 ---
    float co2 = m_airQualitySensor->getCO2();
    UpdateAttribute(CarbonDioxideConcentrationMeasurement::Id, CarbonDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id, co2);

    // --- VOC ---
    float voc = static_cast<float>(m_airQualitySensor->getVOCIndex());
    if (voc < 1.0f) voc = 1.0f;
    if (voc > 500.0f) voc = 500.0f;
    UpdateAttribute(TotalVolatileOrganicCompoundsConcentrationMeasurement::Id,
                    TotalVolatileOrganicCompoundsConcentrationMeasurement::Attributes::MeasuredValue::Id,
                    voc);

    // --- NOx ---
    float nox = static_cast<float>(m_airQualitySensor->getNOxIndex());
    if (nox < 1.0f) nox = 1.0f;
    if (nox > 500.0f) nox = 500.0f;
    UpdateAttribute(NitrogenDioxideConcentrationMeasurement::Id,
                    NitrogenDioxideConcentrationMeasurement::Attributes::MeasuredValue::Id,
                    nox);

    
    //      good, fair, moderate, poor, very poor, extremely poor
    // CO2: <800, <1200, <1500, <2000, <3000, 3000+ 
    // VOC: <100, <150,  <200,  <300,  <400,  <500
    // NO2: <1,   <5,    <20,   <100,  <250,  <500

    using AirQualityEnum = AirQuality::AirQualityEnum;

    auto determineLevel = [](float value, float t0, float t1, float t2, float t3, float t4) -> AirQualityEnum {
        if (value <= t0) return AirQualityEnum::kGood;
        if (value <= t1) return AirQualityEnum::kFair;
        if (value <= t2) return AirQualityEnum::kModerate;
        if (value <= t3) return AirQualityEnum::kPoor;
        if (value <= t4) return AirQualityEnum::kVeryPoor;
        return AirQualityEnum::kExtremelyPoor;
    };

    AirQualityEnum co2Quality = determineLevel(co2, 800, 1200, 1500, 2000, 3000);
    AirQualityEnum vocQuality = determineLevel(voc, 100, 150, 200, 300, 400);
    AirQualityEnum noxQuality = determineLevel(nox, 1, 5, 20, 100, 250);

    AirQualityEnum overallQuality = std::max({ co2Quality, vocQuality, noxQuality });

    UpdateAttribute(AirQuality::Id, AirQuality::Attributes::AirQuality::Id, static_cast<uint8_t>(overallQuality));
}

// --- Helper Implementations ---

// FIX 3: Use esp_matter::endpoint::get_id() instead of m_endpoint->get_endpoint_id()
// m_endpoint is a C-struct/int pointer, not a C++ class with methods.

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, float value) {
    esp_matter_attr_val_t val = esp_matter_float(value);
    
    // LOCK
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
    // UNLOCK
}

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, uint16_t value) {
    esp_matter_attr_val_t val = esp_matter_uint16(value);
    
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, int16_t value) {
    esp_matter_attr_val_t val = esp_matter_int16(value);
    
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}

void MatterAirQualitySensor::UpdateAttribute(uint32_t clusterId, uint32_t attributeId, uint8_t value) {
    esp_matter_attr_val_t val = esp_matter_uint8(value);
    
    chip::DeviceLayer::PlatformMgr().LockChipStack();
    attribute::update(endpoint::get_id(m_endpoint), clusterId, attributeId, &val);
    chip::DeviceLayer::PlatformMgr().UnlockChipStack();
}