#pragma once
#include <Arduino.h>
#include <WiFi.h>
#include <WebServer.h>
#include <SD.h>
#include "Config.h"

// WiFi Web 文件管理服务器（保留 WiFiUploader 兼容别名）
class WebFileManager {
public:
    WebFileManager();
    
    // 启动/停止
    bool start(const char* ssid, const char* password);
    void stop();
    bool isRunning() const { return _running; }
    
    // 获取 IP 地址
    String getIP() const;
    
    // 主循环中调用，处理客户端请求
    void handleClient();
    
    // 获取上传状态
    bool hasNewUpload() const { return _newUpload; }
    String getLastUploadName() const { return _lastUploadName; }
    void clearNewUpload() { _newUpload = false; }
    
private:
    WebServer* _server;
    bool _running;
    bool _newUpload;
    String _lastUploadName;
    File _uploadFile;  // 当前上传的文件句柄
    bool _uploadOk;
    String _uploadPath;
    String _uploadError;
    
    void handleRoot();
    void handleUpload();
    void handleList();
    void handleApiList();
    void handleApiDelete();
    void handleApiMkdir();
    void handleApiStatus();
    void handleDownload();
    void handleNotFound();
    void sendHtmlPage(const char* title, const char* body);
    String normalizePath(const String& path) const;
    String jsonEscape(const String& value) const;
    bool deleteRecursive(const String& path);
};

using WiFiUploader = WebFileManager;
