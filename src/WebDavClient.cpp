#include "WebDavClient.h"
#include <HTTPClient.h>

WebDavClient::WebDavClient() : _port(80), _timeout(5000) {
    memset(_host, 0, sizeof(_host));
    memset(_user, 0, sizeof(_user));
    memset(_pass, 0, sizeof(_pass));
    _basePath[0] = '/';
    _basePath[1] = '\0';
}

bool WebDavClient::config(const char* host, int port, const char* username, const char* password, const char* basePath) {
    strlcpy(_host, host, sizeof(_host));
    _port = port;
    strlcpy(_user, username, sizeof(_user));
    strlcpy(_pass, password, sizeof(_pass));
    if (basePath) {
        strlcpy(_basePath, basePath, sizeof(_basePath));
    }
    return true;
}

bool WebDavClient::get(const char* path, char* outBuffer, size_t outSize, size_t& outLen) {
    if (!WiFi.isConnected()) return false;
    
    HTTPClient http;
    String url = String("http://") + _host + ":" + _port + _basePath + path;
    
    http.setTimeout(_timeout);
    http.begin(url);
    if (_user[0]) {
        http.setAuthorization(_user, _pass);
    }
    
    int code = http.GET();
    if (code != 200) {
        Serial.printf("[WebDav] GET %s failed: %d\n", path, code);
        http.end();
        return false;
    }
    
    String body = http.getString();
    http.end();
    
    outLen = min((size_t)body.length(), outSize - 1);
    memcpy(outBuffer, body.c_str(), outLen);
    outBuffer[outLen] = '\0';
    
    return true;
}

bool WebDavClient::put(const char* path, const char* data, size_t len) {
    if (!WiFi.isConnected()) return false;
    
    HTTPClient http;
    String url = String("http://") + _host + ":" + _port + _basePath + path;
    
    http.setTimeout(_timeout);
    http.begin(url);
    if (_user[0]) {
        http.setAuthorization(_user, _pass);
    }
    
    int code = http.sendRequest("PUT", (uint8_t*)data, len);
    http.end();
    
    if (code != 200 && code != 201 && code != 204) {
        Serial.printf("[WebDav] PUT %s failed: %d\n", path, code);
        return false;
    }
    return true;
}

bool WebDavClient::testConnection() {
    char buf[256];
    size_t len;
    return get("bookProgress.json", buf, sizeof(buf), len);
}