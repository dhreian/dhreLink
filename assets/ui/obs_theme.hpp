// Shared by dhreLink and dhreView. Uses the rich-text subset supported by QLabel.
#pragma once
#include "brand_theme.hpp"
#include <obs-properties.h>
#include <string>
#include <string_view>

namespace dhre::ui {
inline std::string escapeHtml(std::string_view value) {
  std::string result;
  for (const auto character : value) {
    switch (character) {
      case '&': result += "&amp;"; break;
      case '<': result += "&lt;"; break;
      case '>': result += "&gt;"; break;
      case '"': result += "&quot;"; break;
      default: result += character;
    }
  }
  return result;
}

inline obs_property_t* addSection(
  obs_properties_t* properties, const char* id, std::string_view text,
  const char* color = kMutedHex, int pixels = kSmallPixels,
  int weight = kRegularWeight, const char* background = kBlackHex,
  const char* family = kBodyFamilyName) {
  const auto html = std::string("<html><body><table width=\"100%\" cellspacing=\"0\" cellpadding=\"0\" bgcolor=\"") + background +
    "\"><tr><td style=\"padding-left:" + std::to_string(kSpace) +
    "px; padding-right:" + std::to_string(kSpace) +
    "px; padding-top:" + std::to_string(kCompactSpace) +
    "px; padding-bottom:" + std::to_string(kCompactSpace) +
    "px; font-family:'" + family + "'; font-size:" +
    std::to_string(pixels) + "px; font-weight:" + std::to_string(weight) +
    "; color:" + color + ";\">" + escapeHtml(text) + "</td></tr></table></body></html>";
  auto* property = obs_properties_add_text(properties, id, html.c_str(), OBS_TEXT_INFO);
  // Waiting is a normal state. Explicit text and color also work in light OBS themes.
  obs_property_text_set_info_type(property, OBS_TEXT_INFO_NORMAL);
  obs_property_text_set_info_word_wrap(property, true);
  return property;
}

inline void addHeader(obs_properties_t* properties, const char* id, const char* product) {
  addSection(properties, id, product, kWhiteHex, kTitlePixels, kRegularWeight,
    kSurfaceHex, kTitleFamilyName);
}

inline void addStatus(obs_properties_t* properties, const char* id,
                      std::string_view text, bool connected, bool warning = false) {
  addSection(properties, id, text, connected ? kConnectedHex : warning ? kWarningHex : kPurpleHex,
    kBodyPixels, kStrongWeight);
}

inline void addCredit(obs_properties_t* properties, const char* id) {
  addSection(properties, id, "DEVELOPED BY DHREIAN", kPurpleHex, kCreditPixels,
    kRegularWeight, kSurfaceHex);
}
}
