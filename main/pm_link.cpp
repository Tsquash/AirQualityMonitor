#include <Arduino.h>
#include <Preferences.h>
#include <atomic>
#include "pm_link.h"
#include "sense.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"

static const ble_uuid128_t serviceUUID = PM_SERVICE_UUID;
static const ble_uuid128_t valueUUID = PM_VALUE_UUID;
static ble_addr_t savedPeer{}, candidate{};
static std::atomic<bool> paired{false}, pairing{false}, busy{false}, ready{false};
static std::atomic<uint16_t> connection{BLE_HS_CONN_HANDLE_NONE};
static uint16_t serviceStart, serviceEnd, valueHandle, cccdHandle;
static QueueHandle_t readingQueue;
static uint32_t lastAttempt = 0;
static std::atomic<uint32_t> lastTiming{0};
static bool hadFreshReading = false;
static uint8_t missedMinutes = 0;
PMReading remotePM{};

static bool samePeer(const ble_addr_t &a, const ble_addr_t &b) {
    return a.type == b.type && memcmp(a.val, b.val, 6) == 0;
}

// CHIP clears chip-config on every Matter factory reset, including remote resets.
// Only our own key is touched here; other Matter keys are left to Matter.
void clearPMLink() {
    Preferences nvs;
    if (nvs.begin("chip-config", false)) { nvs.remove("pm-peer"); nvs.end(); }
    paired = false;
    remotePM = {};
    Serial.println("[BLE] Saved PM link cleared");
}

static void failLink(int rc) {
    Serial.printf("[BLE] Link setup/transfer failed rc=%d; retrying\n", rc);
    ready = false;
    if (connection != BLE_HS_CONN_HANDLE_NONE)
        ble_gap_terminate(connection, BLE_ERR_REM_USER_CONN_TERM);
    else busy = false;
}

static int timingWritten(uint16_t, const ble_gatt_error *error, ble_gatt_attr *, void *) {
    if (error->status) { failLink(error->status); return 0; }
    if (pairing) {
        Preferences nvs;
        if (!nvs.begin("chip-config", false)) { failLink(BLE_HS_ESTORE_FAIL); return 0; }
        size_t written = nvs.putBytes("pm-peer", &candidate, sizeof(candidate));
        nvs.end();
        if (written != sizeof(candidate)) { failLink(BLE_HS_ESTORE_FAIL); return 0; }
        savedPeer = candidate;
        paired = true;
        pairing = false;
        Serial.println("[BLE] PM link saved in NVS");
    }
    ready = true;
    busy = false;
    return 0;
}

static void writeTiming(uint32_t untilMeasurement) {
    PMTiming timing{untilMeasurement, 0, 0, 0};
    if (untilMeasurement != UINT32_MAX) {
        const auto time = getRTCTime();
        timing.hour = time.hours;
        timing.minute = time.minutes;
        timing.second = time.seconds;
    }
    busy = true;
    int rc = ble_gattc_write_flat(connection, valueHandle, &timing,
                                 sizeof(timing), timingWritten, nullptr);
    if (rc) failLink(rc);
}

static int subscribed(uint16_t, const ble_gatt_error *error, ble_gatt_attr *, void *) {
    if (error->status) failLink(error->status);
    else writeTiming(UINT32_MAX); // Commit pairing; normal loop will supply RTC timing.
    return 0;
}

static int descriptorFound(uint16_t conn, const ble_gatt_error *error, uint16_t,
                           const ble_gatt_dsc *dsc, void *) {
    if (!error->status) {
        if (ble_uuid_u16(&dsc->uuid.u) == 0x2902) cccdHandle = dsc->handle;
    } else if (error->status == BLE_HS_EDONE && cccdHandle) {
        uint16_t notify = 1;
        int rc = ble_gattc_write_flat(conn, cccdHandle, &notify, sizeof(notify), subscribed, nullptr);
        if (rc) failLink(rc);
    } else failLink(error->status);
    return 0;
}

static int characteristicFound(uint16_t conn, const ble_gatt_error *error,
                               const ble_gatt_chr *chr, void *) {
    if (!error->status) valueHandle = chr->val_handle;
    else if (error->status == BLE_HS_EDONE && valueHandle) {
        int rc = ble_gattc_disc_all_dscs(conn, valueHandle, serviceEnd, descriptorFound, nullptr);
        if (rc) failLink(rc);
    } else failLink(error->status);
    return 0;
}

static int serviceFound(uint16_t conn, const ble_gatt_error *error, const ble_gatt_svc *svc, void *) {
    if (!error->status) { serviceStart = svc->start_handle; serviceEnd = svc->end_handle; }
    else if (error->status == BLE_HS_EDONE && serviceStart) {
        int rc = ble_gattc_disc_chrs_by_uuid(conn, serviceStart, serviceEnd, &valueUUID.u, characteristicFound, nullptr);
        if (rc) failLink(rc);
    } else failLink(error->status);
    return 0;
}

static int gapEvent(ble_gap_event *event, void *);

static int gapEvent(ble_gap_event *event, void *) {
    switch (event->type) {
    case BLE_GAP_EVENT_DISC: {
        if (!pairing && !paired) break;
        ble_hs_adv_fields fields{};
        if (ble_hs_adv_parse_fields(&fields, event->disc.data, event->disc.length_data) ||
            fields.svc_data_uuid128_len != 17 ||
            memcmp(fields.svc_data_uuid128, serviceUUID.value, 16)) break;
        // During pairing accept a pairing PM, or retry the candidate if the first
        // pairing acknowledgement was lost after the PM saved us.
        bool eligible = pairing ? (fields.svc_data_uuid128[16] == 1 || samePeer(event->disc.addr, candidate))
                                : samePeer(event->disc.addr, savedPeer);
        if (!eligible) break;
        candidate = event->disc.addr;
        ble_gap_disc_cancel();
        int rc = ble_gap_connect(BLE_OWN_ADDR_PUBLIC, &candidate, 10000, nullptr, gapEvent, nullptr);
        if (rc) failLink(rc);
        break;
    }
    case BLE_GAP_EVENT_DISC_COMPLETE:
        busy = false;
        break;
    case BLE_GAP_EVENT_CONNECT:
        if (event->connect.status) { failLink(event->connect.status); break; }
        connection = event->connect.conn_handle;
        if (!pairing && !paired) { failLink(BLE_HS_EAPP); break; }
        serviceStart = serviceEnd = valueHandle = cccdHandle = 0;
        Serial.println("[BLE] PM connected; discovering measurement service");
        {
            int rc = ble_gattc_disc_svc_by_uuid(connection, &serviceUUID.u, serviceFound, nullptr);
            if (rc) failLink(rc);
        }
        break;
    case BLE_GAP_EVENT_DISCONNECT:
        connection = BLE_HS_CONN_HANDLE_NONE;
        ready = false;
        busy = false;
        lastTiming = 0;
        Serial.printf("[BLE] PM disconnected, reason=%d; retaining readings for three missed minutes\n", event->disconnect.reason);
        break;
    case BLE_GAP_EVENT_NOTIFY_RX: {
        PMReading reading;
        if (paired && ready && event->notify_rx.attr_handle == valueHandle &&
            OS_MBUF_PKTLEN(event->notify_rx.om) == sizeof(reading) &&
            !ble_hs_mbuf_to_flat(event->notify_rx.om, &reading, sizeof(reading), nullptr) &&
            reading.pm1 <= 1000 && reading.pm25 <= 1000 && reading.pm10 <= 1000) {
            xQueueOverwrite(readingQueue, &reading);
            Serial.printf("[BLE] Received PM1=%u PM2.5=%u PM10=%u ug/m3\n", reading.pm1, reading.pm25, reading.pm10);
        } else Serial.println("[BLE] Ignored invalid PM notification");
        break;
    }
    }
    return 0;
}

void initializePMLink(bool pairAtBoot) {
    readingQueue = xQueueCreate(1, sizeof(PMReading));
    if (!readingQueue) { Serial.println("[BLE] Reading queue allocation failed"); return; }
    if (pairAtBoot) clearPMLink();
    Preferences nvs;
    if (nvs.begin("chip-config", false)) {
        paired = nvs.getBytesLength("pm-peer") == sizeof(savedPeer) &&
                 nvs.getBytes("pm-peer", &savedPeer, sizeof(savedPeer)) == sizeof(savedPeer);
        nvs.end();
    }
    pairing = pairAtBoot;
    Serial.printf("[BLE] %s\n", pairing ? "Pairing for 120 seconds" : paired ? "Reconnecting to saved PM" : "No PM linked");
}

void processPMLink(bool rtcReady) {
    if (!readingQueue) return;
    PMReading reading;
    if (xQueueReceive(readingQueue, &reading, 0)) {
        remotePM = reading;
        hadFreshReading = true;
    }
    if (!paired && !pairing) return;
    uint32_t now = millis();
    if (ready && !busy && rtcReady && (!lastTiming || now - lastTiming >= 60000)) {
        auto time = getRTCTime();
        uint32_t seconds = (40 + 60 - time.seconds) % 60;
        if (!seconds) seconds = 60;
        lastTiming = now;
        writeTiming(seconds * 1000);
    }
    if (connection != BLE_HS_CONN_HANDLE_NONE || busy || !ble_hs_synced() ||
        ble_gap_conn_active() || ble_gap_disc_active() || now - lastAttempt < 3000) return;
    lastAttempt = now;
    ble_gap_disc_params params{};
    params.passive = 1;
    params.itvl = 160; // AQM is powered: 50% scan duty while disconnected.
    params.window = 80;
    params.filter_duplicates = 0;
    busy = true;
    int rc = ble_gap_disc(BLE_OWN_ADDR_PUBLIC, 5000, &params, gapEvent, nullptr);
    if (rc) { busy = false; Serial.printf("[BLE] Scan failed rc=%d\n", rc); }
}

void finishPMPairing() {
    pairing = false;
    if (!paired) {
        ble_gap_disc_cancel();
        ble_gap_conn_cancel();
        if (connection != BLE_HS_CONN_HANDLE_NONE)
            ble_gap_terminate(connection, BLE_ERR_REM_USER_CONN_TERM);
    }
    Serial.printf("[BLE] Pairing %s\n", paired ? "succeeded" : "timed out");
}

bool pmLinkPaired() { return paired; }

void finishPMMinute() {
    processPMLink(); // Drain anything received while the CO2 measurement blocked.
    if (hadFreshReading) missedMinutes = 0;
    else if (missedMinutes < 4) ++missedMinutes;
    if (missedMinutes >= 4) remotePM = {};
    hadFreshReading = false;
    Serial.printf("[PM] Report PM1=%u PM2.5=%u PM10=%u; missed=%u/4\n",
                  remotePM.pm1, remotePM.pm25, remotePM.pm10, missedMinutes);
}
