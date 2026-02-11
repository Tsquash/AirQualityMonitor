#include "web_portal.h"

// HTML Templates in PROGMEM
static const char HTML_HEADER[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html>
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, maximum-scale=1, user-scalable=0">
    <title>Air Quality Monitor - Configuration</title>
    <style>
        html, body { margin: 0; padding: 0; font-size: 16px; background: #242424; }
        body, * { box-sizing: border-box; font-family: -apple-system, BlinkMacSystemFont, "Segoe UI", Roboto, "Helvetica Neue", Arial, sans-serif; }
        a { color: inherit; text-decoration: underline; }
        .wrapper { padding: 30px 0; }
        .title-wrapper { max-width: 600px; margin: auto; text-align: center; }
        .title-wrapper h1 { margin-bottom: 5px; font-size: 1.8rem; color: #959595; }
        .title-wrapper p { color: #959595; }
        .container { margin: auto; padding: 40px; max-width: 600px; color: #fff; background: #3a3a3a; box-shadow: 0 0 100px #00000030; border-radius: 8px; }
        .row { margin-bottom: 15px; }
        h1 { margin: 0 0 10px 0; font-family: Arial, sans-serif; font-weight: 300; font-size: 2rem; }
        h1+p { margin-bottom: 30px; }
        h2 { color: #b2b2b2; margin: 30px 0 0 0; font-family: Arial, sans-serif; font-weight: 300; font-size: 1.5rem; }
        h3 { font-family: Arial, sans-serif; font-weight: 300; color: #b2b2b2; font-size: 1.2rem; margin: 25px 0 10px 0; }
        p { font-size: .85rem; margin: 0 0 20px 0; color: #c1c1c1; }
        label { display: block; width: 100%; margin-bottom: 5px; color: #b2b2b2; font-size: 14px; }
        input[type="text"], input[type="number"], input[type="password"], select { display: inline-block; width: 100%; height: 52px; line-height: 50px; padding: 0 15px; color: #fff; border: 1px solid #383838; background: #474747; border-radius: 3px; transition: .15s; box-shadow: none; outline: none; }
        input[type="text"]:hover, input[type="number"]:hover, input[type="password"]:hover, select:hover { border: 1px solid #141414; background: #323232; }
        input[type="text"]:focus, input[type="password"]:focus, select:focus { border: 1px solid #141414; }
        option { color: #fff; }
        button { display: block; width: 100%; padding: 15px 20px; margin-top: 40px; font-size: 1rem; font-weight: 700; text-transform: uppercase; background: #b2b2b2; border: 0; border-radius: 5px; cursor: pointer; transition: .15s; outline: none; }
        button:hover { background: #878787; }
        .github { padding: 15px; text-align: center; }
        .github a { color: inherit; text-decoration: none; transition: .15s; }
        .github a:hover { color: #878787; text-decoration: none; }
        .github p { margin: 0; color: #b2b2b2; }
        .github p span { font-size: 12px; display: inline-block; margin-top: 5px; }
        #dst_wrapper { display: none; }
        input[type="checkbox"] { margin-right: 8px; }
        @media all and (max-width: 620px) {
            .wrapper { padding: 0; }
            .container { padding: 25px 15px; border: 0; border-radius: 0; }
        }
    </style>
</head>
<body>
)rawliteral";

static const char HTML_FOOTER[] PROGMEM = R"rawliteral(
</div>
<div class="github">
<p>
Air Quality Monitor V1.0 | <a href="https://github.com/Tsquash/AirQualityMonitor"</a>
<br>
<span style="">Designed by Caleb Lightfoot 2026</span>
</p>
</div>
</div>
<script>
function toggleDST(){
var c=document.getElementById('dst_enable');
var w=document.getElementById('dst_wrapper');
w.style.display=c.checked?'block':'none';
}
toggleDST();
</script>
</body></html>
)rawliteral";

WebPortal::WebPortal() : server(80), restartFlag(false)
{
}

bool WebPortal::begin()
{
    Serial.println("[WEB] Starting web portal...");

    setupRoutes();
    server.begin();

    // Start DNS server for captive portal
    dnsServer.start(53, "*", WiFi.softAPIP());

    Serial.println("[WEB] Web portal started on port 80");
    return true;
}

void WebPortal::setupRoutes()
{
    // Main page
    server.on("/", HTTP_GET, [this]()
              { handleRoot(); });

    server.on("/", HTTP_POST, [this]()
              { handleFormSubmit(); });

    // Captive portal detection routes
    server.on("/generate_204", HTTP_GET, [this]()
              { handleRoot(); });

    server.on("/fwlink", HTTP_GET, [this]()
              { handleRoot(); });

    server.on("/hotspot-detect.html", HTTP_GET, [this]()
              { handleRoot(); });

    // 404 handler
    server.onNotFound([this]()
                      {
        Serial.printf("[WEB] Request not found: %s %s\n", 
                      server.method() == HTTP_GET ? "GET" : "POST",
                      server.uri().c_str());
        for (int i = 0; i < server.args(); i++) {
            Serial.printf("  Arg %d: %s = %s\n", i, server.argName(i).c_str(), server.arg(i).c_str());
        }
        handleRoot(); });
}

void WebPortal::handleRoot()
{
    String html = getHTMLHeader();
    html += getHTMLFormStart();
    html += getNetworkSettingsHTML();
    html += getBasicSettingsHTML();
    html += getTimezoneSettingsHTML();
    html += getHTMLFormEnd();
    html += getHTMLFooter();

    server.send(200, "text/html", html);
}

void WebPortal::handleFormSubmit()
{
    Serial.println("[WEB] Processing form submission...");

    // Network settings
    if (server.hasArg("ssid"))
    {
        json["ssid"] = server.arg("ssid");
    }
    if (server.hasArg("pass"))
    {
        json["pass"] = server.arg("pass");
    }

    // Basic settings
    if (server.hasArg("hr24_enable"))
    {
        json["hr24_enable"] = server.arg("hr24_enable").toInt();
    }
    if (server.hasArg("unit_c"))
    {
        json["unit_c"] = server.arg("unit_c").toInt();
    }
    if (server.hasArg("enable_matter"))
    {
        json["enable_matter"] = server.arg("enable_matter").toInt();
    }

    // Timezone settings
    if (server.hasArg("std_offset"))
    {
        json["std_offset"] = server.arg("std_offset").toInt();
    }

    // DST settings
    json["dst_enable"] = server.hasArg("dst_enable") ? 1 : 0;

    if (server.hasArg("std_week"))
    {
        json["std_week"] = server.arg("std_week").toInt();
    }
    if (server.hasArg("std_day"))
    {
        json["std_day"] = server.arg("std_day").toInt();
    }
    if (server.hasArg("std_month"))
    {
        json["std_month"] = server.arg("std_month").toInt();
    }
    if (server.hasArg("std_hour"))
    {
        json["std_hour"] = server.arg("std_hour").toInt();
    }

    if (server.hasArg("dst_week"))
    {
        json["dst_week"] = server.arg("dst_week").toInt();
    }
    if (server.hasArg("dst_day"))
    {
        json["dst_day"] = server.arg("dst_day").toInt();
    }
    if (server.hasArg("dst_month"))
    {
        json["dst_month"] = server.arg("dst_month").toInt();
    }
    if (server.hasArg("dst_hour"))
    {
        json["dst_hour"] = server.arg("dst_hour").toInt();
    }
    if (server.hasArg("dst_offset"))
    {
        json["dst_offset"] = server.arg("dst_offset").toInt();
    }

    // Save config
    saveConfig();

    String response = getHTMLHeader();
    response += "<div class=\"wrapper\"><div class=\"container\">";
    response += "<h2 style=\"text-align: center;margin: 0;\">Configuration Saved!</h2>";
    response += "<p style=\"text-align: center;margin: 0;margin-top: 10px;\">Device will restart in about 5 seconds...</p>";
    response += "<script>setTimeout(function(){window.location.href='/';},5000);</script>";
    response += getHTMLFooter();

    json["just_restarted"] = 1;
    saveConfig();

    server.send(200, "text/html", response);

    restartFlag = true;
}

String WebPortal::getHTMLHeader()
{
    return String(FPSTR(HTML_HEADER));
}

String WebPortal::getHTMLFormStart()
{
    return "<div class=\"wrapper\">"
           "<div class=\"title-wrapper\">"
           "<h1>Air Quality Monitor</h1>"
           "</div>"
           "<div class=\"container\">"
           "<form method=\"post\" action=\"/\">";
}

String WebPortal::getHTMLFormEnd()
{
    return "<button type=\"submit\">Save and Reboot</button></form>";
}

String WebPortal::getHTMLFooter()
{
    return String(FPSTR(HTML_FOOTER));
}

String WebPortal::getNetworkSettingsHTML()
{
    String html = "<h2 style=\"margin-top:0;\">Network</h2>"
                  "<p></p>"
                  "<div class=\"row\">"
                  "<label for=\"ssid\">WiFi SSID</label>"
                  "<input type=\"text\" id=\"ssid\" name=\"ssid\" value=\"";
    html += json["ssid"].as<const char *>();
    html += "\"></div>"
            "<div class=\"row\">"
            "<label for=\"pass\">WiFi Password</label>"
            "<input type=\"text\" id=\"pass\" name=\"pass\" value=\"";
    html += json["pass"].as<const char *>();
    html += "\"></div>";

    return html;
}

String WebPortal::getBasicSettingsHTML()
{
    String html = "<h2>General</h2>";
    html += "<p></p>"
            "<div class=\"row\">"
            "<input type=\"checkbox\" id=\"hr24_enable\" name=\"hr24_enable\" value=\"1\"";
    html += (json["hr24_enable"].as<int>() == 1) ? " checked" : "";
    html += "> Enable 24 Hour Time</label></div>";
    html += "<div class=\"row\">"
            "<input type=\"checkbox\" id=\"unit_c\" name=\"unit_c\" value=\"1\"";
    html += (json["unit_c"].as<int>() == 1) ? " checked" : "";
    html += "> Use Celsius</label></div>";
    html += "<div class=\"row\">"
            "<input type=\"checkbox\" id=\"enable_matter\" name=\"enable_matter\" value=\"1\"";
    html += (json["enable_matter"].as<int>() == 1) ? " checked" : "";
    html += "> Enable Matter over Thread</label></div>";
    return html;
}

String WebPortal::getTimezoneSettingsHTML()
{
    String html = "<h2>Timezone</h2>"
                  "<p></p>"
                  "<div class=\"row\"><label for=\"std_offset\">UTC offset in minutes (-660 to 660, CST: -360):</label>"
                  "<input type=\"number\" id=\"std_offset\" name=\"std_offset\" min=\"-660\" max=\"660\" value=\"";
    html += json["std_offset"].isNull() ? "-360" : String(json["std_offset"].as<int>());
    html += "\"></div>"

            "<div class=\"row\"><label>"
            "<input type=\"checkbox\" id=\"dst_enable\" name=\"dst_enable\" value=\"1\" onchange=\"toggleDST()\"";
    html += (json["dst_enable"].as<int>() == 1) ? " checked" : "";
    html += "> Enable Daylight Saving Time</label></div>"

            "<div id=\"dst_wrapper\">"
            "<h3>Standard Time Starts:</h3>"
            "<div class=\"row\"><label for=\"std_week\">Week:</label>"
            "<select id=\"std_week\" name=\"std_week\">"
            "<option value=\"0\"";
    html += (json["std_week"].as<int>() == 0) ? " selected" : "";
    html += ">Last</option>"
            "<option value=\"1\"";
    html += (json["std_week"].isNull() ? 1 : json["std_week"].as<int>()) == 1 ? " selected" : "";
    html += ">First</option>"
            "<option value=\"2\"";
    html += (json["std_week"].as<int>() == 2) ? " selected" : "";
    html += ">Second</option>"
            "<option value=\"3\"";
    html += (json["std_week"].as<int>() == 3) ? " selected" : "";
    html += ">Third</option>"
            "<option value=\"4\"";
    html += (json["std_week"].as<int>() == 4) ? " selected" : "";
    html += ">Fourth</option>"
            "</select></div>"

            "<div class=\"row\"><label for=\"std_day\">Day:</label>"
            "<select id=\"std_day\" name=\"std_day\">"
            "<option value=\"1\"";
    html += (json["std_day"].isNull() ? 1 : json["std_day"].as<int>()) == 1 ? " selected" : "";
    html += ">Sun</option>"
            "<option value=\"2\"";
    html += (json["std_day"].as<int>() == 2) ? " selected" : "";
    html += ">Mon</option>"
            "<option value=\"3\"";
    html += (json["std_day"].as<int>() == 3) ? " selected" : "";
    html += ">Tue</option>"
            "<option value=\"4\"";
    html += (json["std_day"].as<int>() == 4) ? " selected" : "";
    html += ">Wed</option>"
            "<option value=\"5\"";
    html += (json["std_day"].as<int>() == 5) ? " selected" : "";
    html += ">Thu</option>"
            "<option value=\"6\"";
    html += (json["std_day"].as<int>() == 6) ? " selected" : "";
    html += ">Fri</option>"
            "<option value=\"7\"";
    html += (json["std_day"].as<int>() == 7) ? " selected" : "";
    html += ">Sat</option>"
            "</select></div>"

            "<div class=\"row\"><label for=\"std_month\">Month:</label>"
            "<select id=\"std_month\" name=\"std_month\">";

    const char *months[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
    for (int i = 1; i <= 12; i++)
    {
        html += "<option value=\"" + String(i) + "\"";
        int currentMonth = json["std_month"].isNull() ? 11 : json["std_month"].as<int>();
        html += (currentMonth == i) ? " selected" : "";
        html += ">" + String(months[i - 1]) + "</option>";
    }

    html += "</select></div>"

            "<div class=\"row\"><label for=\"std_hour\">Hour (0-23):</label>"
            "<input type=\"number\" id=\"std_hour\" name=\"std_hour\" min=\"0\" max=\"23\" value=\"";
    html += json["std_hour"].isNull() ? "2" : String(json["std_hour"].as<int>());
    html += "\"></div>"

            "<h3>Daylight Saving Time Starts:</h3>"

            "<div class=\"row\"><label for=\"dst_week\">Week:</label>"
            "<select id=\"dst_week\" name=\"dst_week\">"
            "<option value=\"0\"";
    html += (json["dst_week"].as<int>() == 0) ? " selected" : "";
    html += ">Last</option>"
            "<option value=\"1\"";
    html += (json["dst_week"].as<int>() == 1) ? " selected" : "";
    html += ">First</option>"
            "<option value=\"2\"";
    html += (json["dst_week"].isNull() ?  2 : json["dst_week"].as<int>()) == 2 ? " selected" : "";
    html += ">Second</option>"
            "<option value=\"3\"";
    html += (json["dst_week"].as<int>() == 3) ? " selected" : "";
    html += ">Third</option>"
            "<option value=\"4\"";
    html += (json["dst_week"].as<int>() == 4) ? " selected" : "";
    html += ">Fourth</option>"
            "</select></div>"

            "<div class=\"row\"><label for=\"dst_day\">Day:</label>"
            "<select id=\"dst_day\" name=\"dst_day\">"
            "<option value=\"1\"";
    html += (json["dst_day"].isNull() ? 1 : json["dst_day"].as<int>()) == 1 ? " selected" : "";
    html += ">Sun</option>"
            "<option value=\"2\"";
    html += (json["dst_day"].as<int>() == 2) ? " selected" : "";
    html += ">Mon</option>"
            "<option value=\"3\"";
    html += (json["dst_day"].as<int>() == 3) ? " selected" : "";
    html += ">Tue</option>"
            "<option value=\"4\"";
    html += (json["dst_day"].as<int>() == 4) ? " selected" : "";
    html += ">Wed</option>"
            "<option value=\"5\"";
    html += (json["dst_day"].as<int>() == 5) ? " selected" : "";
    html += ">Thu</option>"
            "<option value=\"6\"";
    html += (json["dst_day"].as<int>() == 6) ? " selected" : "";
    html += ">Fri</option>"
            "<option value=\"7\"";
    html += (json["dst_day"].as<int>() == 7) ? " selected" : "";
    html += ">Sat</option>"
            "</select></div>"

            "<div class=\"row\"><label for=\"dst_month\">Month:</label>"
            "<select id=\"dst_month\" name=\"dst_month\">";

    for (int i = 1; i <= 12; i++)
    {
        html += "<option value=\"" + String(i) + "\"";
        int currentMonth = json["dst_month"].isNull() ? 3 : json["dst_month"].as<int>();
        html += (currentMonth == i) ? " selected" : "";
        html += ">" + String(months[i - 1]) + "</option>";
    }

    html += "</select></div>"

            "<div class=\"row\"><label for=\"dst_hour\">Hour (0-23):</label>"
            "<input type=\"number\" id=\"dst_hour\" name=\"dst_hour\" min=\"0\" max=\"23\" value=\"";
    html += json["dst_hour"].isNull() ? "2" : String(json["dst_hour"].as<int>());
    html += "\"></div>"

            "<div class=\"row\"><label for=\"dst_offset\">DST UTC Offset in Minutes (Usually UTC offset +60):</label>"
            "<input type=\"number\" id=\"dst_offset\" name=\"dst_offset\" min=\"-660\" max=\"660\" value=\"";
    html += json["dst_offset"].isNull() ? "-300" : String(json["dst_offset"].as<int>());
    html += "\"></div>"

            "</div>";

    return html;
}

void WebPortal::handleClient()
{
    server.handleClient();
    dnsServer.processNextRequest();
}

bool WebPortal::shouldRestart()
{
    return restartFlag;
}
