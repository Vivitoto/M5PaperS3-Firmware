#pragma once
#include <Arduino.h>
#include <M5Unified.h>

namespace vink3 {

// Upstream baseline used for the v0.3.0 rewrite.
// Source: https://github.com/shinemoon/M5ReadPaper main @ e910d29 (data/version: V1.7.6)
static constexpr const char* kReadPaperUpstreamRepo = "shinemoon/M5ReadPaper";
static constexpr const char* kReadPaperUpstreamCommit = "e910d29";
static constexpr const char* kReadPaperUpstreamVersion = "V1.7.6";

static constexpr int16_t kPaperS3Width = 540;
static constexpr int16_t kPaperS3Height = 960;
static constexpr uint8_t kTextColorDepth = 4;
static constexpr uint8_t kTextColorDepthHigh = 16;

static constexpr uint32_t kDisplayMiddleRefreshThreshold = 8;
static constexpr uint32_t kDisplayQualityFastThreshold = 18;
static constexpr uint32_t kDisplayFullRefreshNormalThreshold = 24;

static constexpr epd_mode_t kQualityRefresh = epd_mode_t::epd_quality;
static constexpr epd_mode_t kMiddleRefresh = epd_mode_t::epd_fast;
static constexpr epd_mode_t kNormalRefresh = epd_mode_t::epd_text;
static constexpr epd_mode_t kLowRefresh = epd_mode_t::epd_fastest;

} // namespace vink3
