#include "LegadoSync.h"

LegadoSync::LegadoSync() : _configured(false) {
}

bool LegadoSync::config(const char* host, int port, const char* username, const char* password) {
    _client.config(host, port, username, password);
    _configured = true;
    return true;
}

bool LegadoSync::testConnection() {
    return _client.testConnection();
}

bool LegadoSync::pullProgress(const char* bookName, LegadoProgress& out) {
    char buf[8192];
    size_t len;
    
    if (!_client.get(PROGRESS_FILE, buf, sizeof(buf), len)) {
        return false;
    }
    
    return findBookInJson(buf, bookName, out);
}

bool LegadoSync::findBookInJson(const char* json, const char* bookName, LegadoProgress& out) {
    DynamicJsonDocument doc(4096);
    DeserializationError err = deserializeJson(doc, json);
    if (err) return false;
    
    JsonArray arr = doc.as<JsonArray>();
    if (arr.isNull()) return false;
    
    for (JsonObject book : arr) {
        const char* url = book["bookUrl"] | "";
        const char* name = book["name"] | "";
        
        if (strstr(url, bookName) || strstr(name, bookName) || strstr(bookName, name)) {
            strlcpy(out.bookUrl, url, sizeof(out.bookUrl));
            strlcpy(out.bookName, name, sizeof(out.bookName));
            out.durChapterIndex = book["durChapterIndex"] | 0;
            out.durChapterPos = book["durChapterPos"] | 0;
            strlcpy(out.durChapterTitle, book["durChapterTitle"] | "", sizeof(out.durChapterTitle));
            out.totalChapterNum = book["totalChapterNum"] | 0;
            return true;
        }
    }
    return false;
}

bool LegadoSync::pushProgress(const LegadoProgress& progress) {
    char buf[8192];
    size_t len;
    bool hasExisting = _client.get(PROGRESS_FILE, buf, sizeof(buf), len);
    
    DynamicJsonDocument doc(4096);
    JsonArray arr;
    
    if (hasExisting) {
        DeserializationError err = deserializeJson(doc, buf);
        if (!err) arr = doc.as<JsonArray>();
    }
    
    if (arr.isNull()) {
        arr = doc.to<JsonArray>();
    }
    
    bool found = false;
    for (JsonObject book : arr) {
        const char* url = book["bookUrl"] | "";
        if (strcmp(url, progress.bookUrl) == 0) {
            book["durChapterIndex"] = progress.durChapterIndex;
            book["durChapterPos"] = progress.durChapterPos;
            book["durChapterTitle"] = progress.durChapterTitle;
            found = true;
            break;
        }
    }
    
    if (!found) {
        JsonVariant v;
        JsonObject obj = v.to<JsonObject>();
        buildProgressJson(progress, obj);
        arr.add(v);
    }
    
    char outBuf[4096];
    size_t outLen = serializeJson(doc, outBuf, sizeof(outBuf));
    return _client.put(PROGRESS_FILE, outBuf, outLen);
}

void LegadoSync::buildProgressJson(const LegadoProgress& progress, JsonObject& obj) {
    obj["bookUrl"] = progress.bookUrl;
    obj["name"] = progress.bookName;
    obj["durChapterIndex"] = progress.durChapterIndex;
    obj["durChapterPos"] = progress.durChapterPos;
    obj["durChapterTitle"] = progress.durChapterTitle;
    obj["totalChapterNum"] = progress.totalChapterNum;
}