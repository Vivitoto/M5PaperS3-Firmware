#pragma once
#include <Arduino.h>
#include <WiFi.h>

// 简化的 WebDav HTTP 客户端
class WebDavClient {
public:
    WebDavClient();
    
    bool config(const char* host, int port, const char* username, const char* password, const char* basePath = nullptr);
    
    bool get(const char* path, char* outBuffer, size_t outSize, size_t& outLen);
    bool put(const char* path, const char* data, size_t len);
    bool testConnection();
    
private:
    char _host[64];
    int _port;
    char _user[32];
    char _pass[32];
    char _basePath[32];
    int _timeout;
};