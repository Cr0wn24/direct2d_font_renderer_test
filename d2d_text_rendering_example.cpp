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
  // hampus: get user default locale name

  wchar_t locale[LOCALE_NAME_MAX_LENGTH] = {};
  if(!GetUserDefaultLocaleName(&locale[0], LOCALE_NAME_MAX_LENGTH))
  {
    memcpy(&locale[0], L"en-US", sizeof(L"en-US"));
  }

  //----------------------------------------------------------
  // hampus: create d2d factory

  ID2D1Factory4 *d2d_factory = 0;
  D2D1_FACTORY_OPTIONS options = {};
  options.debugLevel = D2D1_DEBUG_LEVEL_WARNING;
  hr = D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, options, &d2d_factory);
  ASSERT_HR(hr);

  //----------------------------------------------------------
  // hampus: create rendering params

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

  // const wchar_t *text = L"مرحبا بالعالم";
  // const wchar_t *text = L"🤬";
  const wchar_t *text = L"!=>=-><-=><=🤬😛";
  uint32_t text_length = wcslen(text);
  const wchar_t *font = L"Fira Code";

  MapTextToGlyphsResult map_text_to_glyphs_result = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], font, 50.0f, text, text_length);

  ShowWindow(hwnd, SW_SHOWDEFAULT);

  ID2D1SolidColorBrush *foreground_brush = 0;
  ID2D1RenderTarget *d2d_render_target = 0;
  ID3D11RenderTargetView *render_target_view = 0;
  ID3D11DepthStencilView *depth_stencil_view = 0;
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

      if(d2d_render_target != 0)
      {
        d2d_device_context->Release();
        d2d_render_target->Release();
        foreground_brush->Release();
      }

      if(render_target_view != 0)
      {
        context->ClearState();
        render_target_view->Release();
        depth_stencil_view->Release();
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

        ID3D11Texture2D *depth = 0;
        device->CreateTexture2D(&depth_desc, 0, &depth);
        device->CreateDepthStencilView((ID3D11Resource *)depth, 0, &depth_stencil_view);
        depth->Release();
      }

      //----------------------------------------------------------
      // hampus: recreate d2d render target

      D2D1_RENDER_TARGET_PROPERTIES props =
      {
        .type = D2D1_RENDER_TARGET_TYPE_DEFAULT,
        .pixelFormat = {DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_IGNORE},
      };

      ID3D11Texture2D *backbuffer = 0;
      swap_chain->GetBuffer(0, IID_ID3D11Texture2D, (void **)&backbuffer);

      IDXGISurface *surface = {};
      hr = backbuffer->QueryInterface(__uuidof(IDXGISurface), (void **)&surface);
      ASSERT_HR(hr);

      hr = d2d_factory->CreateDxgiSurfaceRenderTarget(surface, &props, &d2d_render_target);
      ASSERT_HR(hr);

      d2d_render_target->SetTextRenderingParams(rendering_params);

      D2D1_COLOR_F foreground_color = {1, 1, 1, 1};

      hr = d2d_render_target->CreateSolidColorBrush(&foreground_color, 0, &foreground_brush);
      ASSERT_HR(hr);

      hr = d2d_render_target->QueryInterface(__uuidof(ID2D1DeviceContext4), (void **)&d2d_device_context);
      ASSERT_HR(hr);

      backbuffer->Release();
      surface->Release();

      current_width = width;
      current_height = height;
    }

    // hampus: draw

    if(render_target_view)
    {
      d2d_render_target->BeginDraw();
      D2D1_COLOR_F clear_color = {0.392f, 0.584f, 0.929f, 1.f};
      d2d_render_target->Clear(clear_color);
      const MapTextToGlyphsResult &result = map_text_to_glyphs_result;
      float advance = 0;
      for(TextToGlyphsSegment *segment = result.first_segment; segment != 0; segment = segment->next)
      {
        IDWriteColorGlyphRunEnumerator1 *run_enumerator = 0;
        const DWRITE_GLYPH_IMAGE_FORMATS desired_glyph_image_formats = DWRITE_GLYPH_IMAGE_FORMATS_TRUETYPE |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_CFF |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_COLR |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_SVG |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_PNG |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_JPEG |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_TIFF |
                                                                       DWRITE_GLYPH_IMAGE_FORMATS_PREMULTIPLIED_B8G8R8A8;
        hr = dwrite_factory->TranslateColorGlyphRun({0, 0}, &segment->dwrite_glyph_run, 0, desired_glyph_image_formats, DWRITE_MEASURING_MODE_NATURAL, 0, 0, &run_enumerator);
        if(hr == DWRITE_E_NOCOLOR)
        {
          // NOTE(hampus): There was no colored glyph. We can draw them as normal

          foreground_brush->SetColor({1, 1, 1, 1});
          d2d_device_context->DrawGlyphRun({100 + advance, 100}, &segment->dwrite_glyph_run, foreground_brush, DWRITE_MEASURING_MODE_NATURAL);
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

            DWRITE_COLOR_GLYPH_RUN1 const *color_glyph_run = 0;
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
                // TODO(hampus): This is not tested.
                d2d_device_context->DrawColorBitmapGlyphRun(color_glyph_run->glyphImageFormat, {0, 0}, &color_glyph_run->glyphRun, color_glyph_run->measuringMode, D2D1_COLOR_BITMAP_GLYPH_SNAP_OPTION_DEFAULT);
              }
              break;
              case DWRITE_GLYPH_IMAGE_FORMATS_SVG:
              {
                // TODO(hampus): This is not tested.
                d2d_device_context->DrawSvgGlyphRun({100, 100}, &color_glyph_run->glyphRun, foreground_brush, 0, 0, color_glyph_run->measuringMode);
              }
              break;
              default:
              {
                d2d_device_context->DrawGlyphRun({100 + advance, 100}, &color_glyph_run->glyphRun, foreground_brush, DWRITE_MEASURING_MODE_NATURAL);
              }
              break;
            }

            ASSERT_HR(hr);
          }
        }

        // hampus: advance

        for(int glyph_idx = 0; glyph_idx < segment->glyph_array.count; ++glyph_idx)
        {
          advance += segment->glyph_array.advances[glyph_idx];
        }
      }
      hr = d2d_render_target->EndDraw();
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

  d2d_device_context->Release();
  d2d_render_target->Release();
  foreground_brush->Release();
  d2d_factory->Release();

  return 0;
}