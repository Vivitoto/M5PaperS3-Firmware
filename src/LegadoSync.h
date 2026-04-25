#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>
#include "WebDavClient.h"

// Legado 阅读进度同步
struct LegadoProgress {
    char bookUrl[128];
    char bookName[64];
    int durChapterIndex;
    int durChapterPos;
    char durChapterTitle[64];
    int totalChapterNum;
};

class LegadoSync {
public:
    LegadoSync();
    
    bool config(const char* host, int port, const char* username, const char* password);
    bool testConnection();
    bool pullProgress(const char* bookName, LegadoProgress& out);
    bool pushProgress(const LegadoProgress& progress);
    bool isConfigured() const { return _configured; }
    
private:
    WebDavClient _client;
    bool _configured;
    
    static constexpr const char* PROGRESS_FILE = "bookProgress.json";
    bool findBookInJson(const char* json, const char* bookName, LegadoProgress& out);
    void buildProgressJson(const LegadoProgress& progress, JsonObject& obj);
};