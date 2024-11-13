#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <dxgidebug.h>
#include <d2d1_3.h>

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "d2d1.lib")

#include "dwrite_text_to_glyphs.h"

////////////////////////////////////////////////////////////
// hampus: window proc callback

static LRESULT CALLBACK
window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  LRESULT result = 0;
  switch(message)
  {
    case WM_DESTROY:
    {
      PostQuitMessage(0);
    }
    break;
    default:
    {
      result = DefWindowProcW(hwnd, message, wparam, lparam);
    }
    break;
  }
  return result;
}

////////////////////////////////////////////////////////////
// hampus: entry point

int APIENTRY
WinMain(HINSTANCE instance, HINSTANCE prev_instance, PSTR command_line, int show_code)
{
  //----------------------------------------------------------
  // hampus: create window

  HWND hwnd = 0;
  {
    WNDCLASSW window_class = {};
    window_class.lpfnWndProc = window_proc;
    window_class.hInstance = instance;
    window_class.lpszClassName = L"ApplicationWindowClassName";
    window_class.style = CS_VREDRAW | CS_HREDRAW;
    window_class.hCursor = LoadCursor(0, IDC_ARROW);

    ATOM register_class_result = RegisterClassW(&window_class);
    if(register_class_result == 0)
    {
      OutputDebugStringA("RegisterClassW failed");
      return 1;
    }

    hwnd = CreateWindowExW(0,
                           window_class.lpszClassName,
                           L"Direct2D font renderer test",
                           WS_OVERLAPPEDWINDOW,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           CW_USEDEFAULT, CW_USEDEFAULT,
                           0, 0, instance, 0);
    if(hwnd == 0)
    {
      OutputDebugStringA("CreateWindowExW failed");
      return 1;
    }
  }

  HRESULT hr = {};

  //----------------------------------------------------------
  // hampus: create d3d11 device & context

  ID3D11Device *device = 0;
  ID3D11DeviceContext *context = 0;
  {
    UINT flags = 0;
#ifndef NDEBUG
    flags |= D3D11_CREATE_DEVICE_DEBUG;
#endif
    // NOTE(hampus): thos flag is required for CreateDxgiSurfaceRenderTarget later on.
    // see https://learn.microsoft.com/en-us/windows/win32/api/d2d1/nf-d2d1-id2d1factory-createdxgisurfacerendertarget(idxgisurface_constd2d1_render_target_properties__id2d1rendertarget)

    flags |= D3D11_CREATE_DEVICE_BGRA_SUPPORT;
    D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0};
    hr = D3D11CreateDevice(0, D3D_DRIVER_TYPE_HARDWARE, 0, flags, levels, ARRAYSIZE(levels),
                           D3D11_SDK_VERSION, &device, 0, &context);
    ASSERT_HR(hr);
  }

  //----------------------------------------------------------
  // hampus: enable useful breaks on d3d11 errors

  {
    IDXGIInfoQueue *dxgi_info = 0;
    ID3D11InfoQueue *info = 0;
#ifndef NDEBUG
    hr = device->QueryInterface(IID_ID3D11InfoQueue, (void **)&info);
    ASSERT_HR(hr);
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    info->SetBreakOnSeverity(D3D11_MESSAGE_SEVERITY_ERROR, TRUE);
    info->Release();

    hr = DXGIGetDebugInterface1(0, IID_IDXGIInfoQueue, (void **)&dxgi_info);
    ASSERT_HR(hr);
    dxgi_info->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_CORRUPTION, TRUE);
    dxgi_info->SetBreakOnSeverity(DXGI_DEBUG_ALL, DXGI_INFO_QUEUE_MESSAGE_SEVERITY_ERROR, TRUE);
    dxgi_info->Release();
#endif
  }

  //----------------------------------------------------------
  // hampus: create dxgi swap chain

  IDXGISwapChain1 *swap_chain = 0;
  IDXGIDevice *dxgi_device = 0;
  {
    IDXGIAdapter *dxgi_adapter = 0;
    IDXGIFactory2 *factory = 0;

    hr = device->QueryInterface(IID_IDXGIDevice, (void **)&dxgi_device);
    ASSERT_HR(hr);

    hr = dxgi_device->GetAdapter(&dxgi_adapter);
    ASSERT_HR(hr);

    hr = dxgi_adapter->GetParent(IID_IDXGIFactory2, (void **)&factory);
    ASSERT_HR(hr);
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    {
      desc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
      desc.Stereo = FALSE;
      desc.SampleDesc.Count = 1;
      desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
      desc.BufferCount = 2;
      desc.Scaling = DXGI_SCALING_STRETCH;
      desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
      desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
    };

    hr = factory->CreateSwapChainForHwnd(device, hwnd, &desc, 0, 0, &swap_chain);
    ASSERT_HR(hr);

    factory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

    factory->Release();
    dxgi_adapter->Release();
  }

  //----------------------------------------------------------
  // hampus: create dwrite factory

  IDWriteFactory4 *dwrite_factory = 0;
  hr = DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED,
                           __uuidof(IDWriteFactory4),
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
  // hampus: create d2d factory

  ID2D1Factory5 *d2d_factory = 0;
  D2D1_FACTORY_OPTIONS options = {};
  options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2d_factory);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: create d2d device

  ID2D1Device4 *d2d_device = 0;

  hr = d2d_factory->CreateDevice(dxgi_device, &d2d_device);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: get user default locale name

  wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
  if(!GetUserDefaultLocaleName(&locale[0], LOCALE_NAME_MAX_LENGTH))
  {
    memcpy(&locale[0], L"en-US", sizeof(L"en-US"));
  }

  //----------------------------------------------------------
  // hampus: create rendering params

  // NOTE(hampus): Since multiple monitors may have different rendering settings,
  // this has to be reset if the monitor where to change. We can look for the
  // WM_WINDOWPOSCHANGED message and check if we actually have changed monitor.

  IDWriteRenderingParams *base_rendering_params = 0;
  IDWriteRenderingParams *rendering_params = 0;

  hr = dwrite_factory->CreateRenderingParams(&base_rendering_params);
  ASSERT_HR(hr);

  FLOAT gamma = base_rendering_params->GetGamma();
  FLOAT enhanced_contrast = base_rendering_params->GetEnhancedContrast();
  FLOAT clear_type_level = base_rendering_params->GetClearTypeLevel();
  hr = dwrite_factory->CreateCustomRenderingParams(gamma,
                                                   enhanced_contrast,
                                                   clear_type_level,
                                                   base_rendering_params->GetPixelGeometry(),
                                                   base_rendering_params->GetRenderingMode(),
                                                   &rendering_params);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: map text to glyphs

  // const wchar_t *ligatures_text = L"Ligatures: !=>=-><-=><=";
  const wchar_t *emojis_text = L"😀a";
  // const wchar_t *text = L"Hello world";
  // const wchar_t *arabic_text = L"Arabic text: مرحبا بالعالم";

  const wchar_t *font = L"Fira Code";

  MapTextToGlyphsResult text_to_glyphs_results[] =
  {
    // dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], font, 50.0f, ligatures_text, wcslen(ligatures_text)),
    dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], font, 50.0f, emojis_text, wcslen(emojis_text)),
    // dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], font, 50.0f, text, wcslen(text)),
    // dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], font, 50.0f, arabic_text, wcslen(arabic_text)),
  };

  wchar_t filePaths[8][MAX_PATH] = {};
  int filePathsCount = 0;

  for(TextToGlyphsSegmentNode *n = text_to_glyphs_results[0].first_segment; n != 0; n = n->next)
  {
    UINT32 numberOfFiles;
    n->v.font_face->GetFiles(&numberOfFiles, nullptr);
    IDWriteFontFile *fontFiles[8] = {};
    n->v.font_face->GetFiles(&numberOfFiles, &fontFiles[0]);

    IDWriteFontFileLoader *loader = nullptr;
    fontFiles[0]->GetLoader(&loader);

    void const *fontFileReferenceKey;
    UINT32 fontFileReferenceKeySize;
    fontFiles[0]->GetReferenceKey(&fontFileReferenceKey, &fontFileReferenceKeySize);

    IDWriteLocalFontFileLoader *localLoader = nullptr;
    loader->QueryInterface(&localLoader);
    if(localLoader)
    {
      UINT32 filePathLength;
      localLoader->GetFilePathLengthFromKey(fontFileReferenceKey, fontFileReferenceKeySize, &filePathLength);
      filePathLength++;
      localLoader->GetFilePathFromKey(fontFileReferenceKey, fontFileReferenceKeySize, &filePaths[filePathsCount][0], filePathLength);
      filePathsCount++;
    }
  }

  ShowWindow(hwnd, SW_SHOWDEFAULT);

  ID2D1SolidColorBrush *foreground_brush = 0;
  ID3D11RenderTargetView *render_target_view = 0;
  ID2D1DeviceContext4 *d2d_device_context = 0;

  DWORD current_width = 0;
  DWORD current_height = 0;

  //----------------------------------------------------------
  // hampus: main loop

  bool running = true;
  while(running)
  {
    for(MSG message; PeekMessageW(&message, 0, 0, 0, PM_REMOVE);)
    {
      if(message.message == WM_QUIT)
      {
        running = false;
      }
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    RECT rect = {};
    GetClientRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    // hampus: resize render target view if size changed

    if(render_target_view == 0 || width != current_width || height != current_height)
    {
      if(d2d_device_context != 0)
      {
        d2d_device_context->Release();
        foreground_brush->Release();
      }

      if(render_target_view != 0)
      {
        context->ClearState();
        render_target_view->Release();
        render_target_view = 0;
      }

      if(width != 0 && height != 0)
      {
        hr = swap_chain->ResizeBuffers(0, width, height, DXGI_FORMAT_UNKNOWN, 0);
        if(FAILED(hr))
        {
          ASSERT("Failed to resize swap chain!");
        }

        ID3D11Texture2D *backbuffer = 0;
        swap_chain->GetBuffer(0, IID_ID3D11Texture2D, (void **)&backbuffer);
        device->CreateRenderTargetView((ID3D11Resource *)backbuffer, 0, &render_target_view);
        backbuffer->Release();

        D3D11_TEXTURE2D_DESC depth_desc =
        {
          .Width = (UINT)width,
          .Height = (UINT)height,
          .MipLevels = 1,
          .ArraySize = 1,
          .Format = DXGI_FORMAT_D32_FLOAT,
          .SampleDesc = {1, 0},
          .Usage = D3D11_USAGE_DEFAULT,
          .BindFlags = D3D11_BIND_DEPTH_STENCIL,
        };
      }

      //----------------------------------------------------------
      // hampus: recreate d2d render target

      D2D1_DEVICE_CONTEXT_OPTIONS options = {};
      hr = d2d_device->CreateDeviceContext(options, &d2d_device_context);
      ASSERT_HR(hr);

      ID3D11Texture2D *backbuffer = 0;
      swap_chain->GetBuffer(0, IID_ID3D11Texture2D, (void **)&backbuffer);

      IDXGISurface *surface = {};
      hr = backbuffer->QueryInterface(__uuidof(IDXGISurface), (void **)&surface);
      ASSERT_HR(hr);

      UINT dpi = GetDpiForWindow(hwnd);

      bool use_cleartype = true;
      D2D1_BITMAP_PROPERTIES1 bitmapProperties = {};
      bitmapProperties.bitmapOptions = D2D1_BITMAP_OPTIONS_TARGET | D2D1_BITMAP_OPTIONS_CANNOT_DRAW;
      bitmapProperties.pixelFormat.format = DXGI_FORMAT_B8G8R8A8_UNORM;
      bitmapProperties.pixelFormat.alphaMode = use_cleartype ? D2D1_ALPHA_MODE_IGNORE : D2D1_ALPHA_MODE_PREMULTIPLIED;
      bitmapProperties.dpiY = dpi;
      bitmapProperties.dpiX = dpi;

      ID2D1Bitmap1 *bitmap = 0;
      hr = d2d_device_context->CreateBitmapFromDxgiSurface(surface, bitmapProperties, &bitmap);
      ASSERT_HR(hr);

      d2d_device_context->SetTarget(bitmap);

      d2d_device_context->SetTextRenderingParams(rendering_params);

      D2D1_COLOR_F foreground_color = {1, 1, 1, 1};
      hr = d2d_device_context->CreateSolidColorBrush(&foreground_color, 0, &foreground_brush);
      ASSERT_HR(hr);

      bitmap->Release();
      surface->Release();
      backbuffer->Release();

      current_width = width;
      current_height = height;
    }

    // hampus: draw

    // NOTE(hampus): This won't draw arabic sentences correctly if our base
    // font doesn't have arabic. Because in that case, each space in the sentence
    // will fall back to our base font while the arabic words will fall back
    // to another font, thus creating multiple segments and thus drawing
    // the words in reverse order. This won't be a problem however if you do
    // your own layouting of the characters. It is only a problem if you draw
    // entire sentences to the screen directly with DrawGlyphRun().

    if(render_target_view)
    {
      d2d_device_context->BeginDraw();
      D2D1_COLOR_F clear_color = {0.392f, 0.584f, 0.929f, 1.f};
      d2d_device_context->Clear(clear_color);
      float advance_x = 0;
      float advance_y = 0;
      for(int result_idx = 0; result_idx < ARRAYSIZE(text_to_glyphs_results); ++result_idx)
      {
        const MapTextToGlyphsResult &result = text_to_glyphs_results[result_idx];
        float max_advance_for_this_result = 0;
        for(TextToGlyphsSegmentNode *n = result.first_segment; n != 0; n = n->next)
        {
          TextToGlyphsSegment &segment = n->v;
          DWRITE_GLYPH_RUN dwrite_glyph_run = {};
          dwrite_glyph_run.glyphCount = segment.glyph_count;
          dwrite_glyph_run.fontEmSize = segment.font_size_em;
          dwrite_glyph_run.fontFace = segment.font_face;
          dwrite_glyph_run.bidiLevel = segment.bidi_level;

          dwrite_glyph_run.glyphAdvances = segment.glyph_advances;
          dwrite_glyph_run.glyphIndices = segment.glyph_indices;
          dwrite_glyph_run.glyphOffsets = segment.glyph_offsets;

          if(segment.bidi_level != 0)
          {
            // NOTE(hampus): The segment is right to left. We will advance
            // beforehand so the lower-left corner of the text is the baseline.
            // Otherwise the lower-right corner of the text would be the baseline
            // which would render into our left to right text before it if we had any.
            for(int glyph_idx = 0; glyph_idx < segment.glyph_count; ++glyph_idx)
            {
              advance_x += segment.glyph_advances[glyph_idx];
            }
          }

          D2D1_POINT_2F baseline = {};
          baseline.x = 50 + advance_x;
          baseline.y = 50 + advance_y;

          D2D1_RECT_F glyph_world_bounds = {};
          d2d_device_context->GetGlyphRunWorldBounds(baseline, &dwrite_glyph_run, DWRITE_MEASURING_MODE_NATURAL, &glyph_world_bounds);
          bool is_whitespace = !(glyph_world_bounds.right > glyph_world_bounds.left && glyph_world_bounds.bottom > glyph_world_bounds.top);

          IDWriteColorGlyphRunEnumerator1 *run_enumerator = 0;
          const DWRITE_GLYPH_IMAGE_FORMATS desired_glyph_image_formats = DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_CFF |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_COLR |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_SVG |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_PNG |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
                                                                         DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;
          hr = dwrite_factory->TranslateColorGlyphRun(baseline, &dwrite_glyph_run, 0, desired_glyph_image_formats, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &run_enumerator);
          if(hr == DWRITE_E_NOCOLOR)
          {
            // NOTE(hampus): There was no colored glyph. We can draw them as normal

            foreground_brush->SetColor({1, 1, 1, 1});
            d2d_device_context->DrawGlyphRun(baseline, &dwrite_glyph_run, foreground_brush, DWRITE_MEASURING_MODE_NATURAL);
          }
          else
          {
            // NOTE(hampus): There was colored glyph. We have to draw them differently
            for(;;)
            {
              BOOL have_run = FALSE;
              hr = run_enumerator->MoveNext(&have_run);
              ASSERT_HR(hr);
              if(!have_run)
              {
                break;
              }

              const DWRITE_COLOR_GLYPH_RUN1 *color_glyph_run = 0;
              hr = run_enumerator->GetCurrentRun(&color_glyph_run);
              ASSERT_HR(hr);

              foreground_brush->SetColor(color_glyph_run->runColor);

              switch(color_glyph_run->glyphImageFormat)
              {
                case DWRITE_GLYPH_IMAGE_FORMATS_NONE:
                {
                  // NOTE(hampus): Do nothing
                  // TODO(hampus): Find out when this is the case.
                }
                break;
                case DWRITE_GLYPH_IMAGE_FORMATS_PNG:
                case DWRITE_GLYPH_IMAGE_FORMATS_JPEG:
                case DWRITE_GLYPH_IMAGE_FORMATS_TIFF:
                case DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8:
                {
                  ASSERT(!"Not tested");
                  d2d_device_context->DrawColorBitmapGlyphRun(color_glyph_run->glyphImageFormat, {color_glyph_run->baselineOriginX, color_glyph_run->baselineOriginY}, &color_glyph_run->glyphRun, color_glyph_run->measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
                }
                break;
                case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
                {
                  ASSERT(!"Not tested");
                  d2d_device_context->DrawSvgGlyphRun({color_glyph_run->baselineOriginX, color_glyph_run->baselineOriginY}, &color_glyph_run->glyphRun, foreground_brush, 0, 0, color_glyph_run->measuringMode);
                }
                break;
                default:
                {
                  d2d_device_context->DrawGlyphRun({color_glyph_run->baselineOriginX, color_glyph_run->baselineOriginY}, &color_glyph_run->glyphRun, color_glyph_run->glyphRunDescription, foreground_brush, color_glyph_run->measuringMode);
                }
                break;
              }

              ASSERT_HR(hr);
            }
          }

          // hampus: advance

          DWRITE_FONT_METRICS font_metrics = {};
          segment.font_face->GetMetrics(&font_metrics);
          float advance_y_for_this = (font_metrics.ascent + font_metrics.descent + font_metrics.lineGap) * segment.font_size_em / font_metrics.designUnitsPerEm;
          max_advance_for_this_result = max(max_advance_for_this_result, advance_y_for_this);

          if(segment.bidi_level == 0)
          {
            for(int glyph_idx = 0; glyph_idx < segment.glyph_count; ++glyph_idx)
            {
              advance_x += segment.glyph_advances[glyph_idx];
            }
          }
        }
        advance_y += max_advance_for_this_result;
        advance_x = 0;
      }
      hr = d2d_device_context->EndDraw();
      ASSERT_HR(hr);
    }

    // hampus: present

    BOOL vsync = TRUE;
    hr = swap_chain->Present(vsync ? 1 : 0, 0);
    if(hr == DXGI_STATUS_OCCLUDED)
    {
      if(vsync)
      {
        Sleep(10);
      }
    }
    else if(FAILED(hr))
    {
      ASSERT("Failed to present swap chain! Device lost?");
    }
  }

  foreground_brush->Release();
  d2d_device_context->Release();
  d2d_device->Release();
  d2d_factory->Release();

  return 0;
}