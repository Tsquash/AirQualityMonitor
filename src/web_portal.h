#ifndef WEB_PORTAL_H
#define WEB_PORTAL_H

#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>
#include "utils.h"

class WebPortal {
public:
    WebPortal();
    bool begin();
    void handleClient();
    bool shouldRestart();
    
private:
    WebServer server;
    DNSServer dnsServer;
    bool restartFlag;
    
    void setupRoutes();
    void handleRoot();
    void handleFormSubmit();
    
    String getHTMLHeader();
    String getHTMLFormStart();
    String getHTMLFormEnd();
    String getHTMLFooter();
    
    String getNetworkSettingsHTML();
    String getBasicSettingsHTML();
    String getTimezoneSettingsHTML();
};

#endif // WEB_PORTAL_H
