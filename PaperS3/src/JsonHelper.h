#pragma once
#include <Arduino.h>
#include <ArduinoJson.h>

// ArduinoJson 6.21 兼容 helper
// 6.21 中 JsonArray::add<JsonObject>() 不能直接调用，用 JsonVariant 包装
inline JsonObject addJsonObject(JsonArray& arr) {
    JsonVariant v;
    JsonObject obj = v.to<JsonObject>();
    arr.add(v);
    return obj;
}
