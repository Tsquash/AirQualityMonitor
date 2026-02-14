#ifndef MATTER_AIR_QUALITY_SENSOR_H
#define MATTER_AIR_QUALITY_SENSOR_H

#include <esp_matter.h>
#include <app/clusters/air-quality-server/air-quality-server.h>

struct AirQualitySensor {
    float getTemperature();
    float getHumidity();
    float getCO2();
    int32_t getVOCIndex(); // Added for VOC readings
    int32_t getNOxIndex(); // Added for NOx readings
};

// Use the standard namespaces to fix "AirQualityEnum not declared"
using namespace esp_matter;
using namespace chip::app::Clusters;

class MatterAirQualitySensor {
public:
    // Factory method to create the sensor
    static std::shared_ptr<MatterAirQualitySensor> CreateEndpoint(
        node_t* node, // Changed from custom MatterNode to standard node_t
        std::shared_ptr<AirQualitySensor> airQualitySensor);

    void UpdateMeasurements();

    // Constructor must be public for std::make_shared
    MatterAirQualitySensor(endpoint_t* endpoint, std::shared_ptr<AirQualitySensor> airQualitySensor);

private:
    endpoint_t* m_endpoint;
    std::shared_ptr<AirQualitySensor> m_airQualitySensor;

    // Helper functions to replace the missing Base class functionality
    void UpdateAttribute(uint32_t clusterId, uint32_t attributeId, float value);
    void UpdateAttribute(uint32_t clusterId, uint32_t attributeId, uint16_t value);
    void UpdateAttribute(uint32_t clusterId, uint32_t attributeId, int16_t value);
    void UpdateAttribute(uint32_t clusterId, uint32_t attributeId, uint8_t value);
};

#endif