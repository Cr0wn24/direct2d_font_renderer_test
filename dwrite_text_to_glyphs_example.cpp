#pragma warning(push, 0)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#pragma warning(pop)

#pragma comment(lib, "dwrite.lib")

#include "dwrite_text_to_glyphs.h"

int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR command_line, int show_code)
{
  HRESULT hr = 0;

  //----------------------------------------------------------
  // hampus: create dwrite factory

  IDWriteFactory2 *dwrite_factory = 0;
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                           __uuidof(IDWriteFactory2),
                           (IUnknown **)&dwrite_factory);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: create dwrite font fallback

  IDWriteFontFallback *font_fallback = 0;
  hr = dwrite_factory->GetSystemFontFallback(&font_fallback);
  ASSERT_HR(hr);

  IDWriteFontFallback1 *font_fallback1 = 0;
  hr = font_fallback->QueryInterface(__uuidof(IDWriteFontFallback1), (void **)&font_fallback1);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: create dwrite font collection

  IDWriteFontCollection *font_collection = 0;
  hr = dwrite_factory->GetSystemFontCollection(&font_collection);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: create dwrite text analyzer

  IDWriteTextAnalyzer *text_analyzer = 0;
  hr = dwrite_factory->CreateTextAnalyzer(&text_analyzer);
  ASSERT_HR(hr);

  IDWriteTextAnalyzer1 *text_analyzer1 = 0;
  hr = text_analyzer->QueryInterface(__uuidof(IDWriteTextAnalyzer1), (void **)&text_analyzer1);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: get user default locale name

  wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
  if(!GetUserDefaultLocaleName(&locale[0], LOCALE_NAME_MAX_LENGTH))
  {
    memcpy(&locale[0], L"en-US", sizeof(L"en-US"));
  }

  MapTextToGlyphsResult map_text_to_glyphs_result = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], L"Segoe UI", 16.0f, L"Hello->world", wcslen(L"Hello->world"));

  return 0;
}