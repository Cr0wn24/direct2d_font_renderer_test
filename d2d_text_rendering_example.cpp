#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_3.h>
#include <d3dcompiler.h>
#include <dxgidebug.h>

#pragma comment(lib, "dxguid")
#pragma comment(lib, "dxgi")
#pragma comment(lib, "d3d11")
#pragma comment(lib, "d3dcompiler")
#pragma comment(lib, "dwrite.lib")

#include "dwrite_text_to_glyphs.h"

struct Vec2F32
{
  float x;
  float y;
};

struct Vec4F32
{
  float r;
  float g;
  float b;
  float a;
};

struct Vertex
{
  Vec2F32 position;
  Vec2F32 uv;
  Vec4F32 color;
};

static bool g_running;

////////////////////////////////////////////////////////////
// hampus: basic helpers

static Vec2F32
v2f32(float x, float y)
{
  Vec2F32 result = {};
  result.x = x;
  result.y = y;
  return result;
}

static Vec4F32
v4f32(float r, float g, float b, float a)
{
  Vec4F32 result = {};
  result.r = r;
  result.g = g;
  result.b = g;
  result.a = a;
  return result;
}

static Vec2F32
ndc_space_from_pixels_space(Vec2F32 render_dim, Vec2F32 pos_px)
{
  Vec2F32 result = {};
  result.x = pos_px.x / render_dim.x * 2 - 1;
  result.y = (render_dim.y - pos_px.y) / render_dim.y * 2 - 1;
  return result;
}

////////////////////////////////////////////////////////////
// hampus: window proc callback

static LRESULT CALLBACK
window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
  LRESULT result = 0;
  switch(message)
  {
    case WM_QUIT:
    {
      g_running = false;
    }
    break;
    case WM_DESTROY:
    {
      g_running = false;
    }
    break;
    case WM_CLOSE:
    {
      g_running = false;
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
  {
    IDXGIDevice *dxgi_device = 0;
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
      desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
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
    dxgi_device->Release();
  }

  //----------------------------------------------------------
  // hampus: create d3d11 vertex buffer

  uint32_t texture_atlas_width = 512;
  uint32_t texture_atlas_height = 512;
  uint32_t texture_atlas_pitch = texture_atlas_width * sizeof(uint32_t);
  uint8_t *texture_atlas_data = (uint8_t *)calloc(texture_atlas_height * texture_atlas_height * 4, 1);

  ID3D11Buffer *vertex_buffer = 0;
  {
    D3D11_BUFFER_DESC desc =
    {
      .ByteWidth = sizeof(Vertex) * 4,
      .Usage = D3D11_USAGE_DYNAMIC,
      .BindFlags = D3D11_BIND_VERTEX_BUFFER,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };

    device->CreateBuffer(&desc, 0, &vertex_buffer);
  }

  //----------------------------------------------------------
  // hampus: create d3d11 vertex and pixel shader

  ID3D11InputLayout *layout = 0;
  ID3D11VertexShader *vertex_shader = 0;
  ID3D11PixelShader *pixel_shader = 0;
  {
    D3D11_INPUT_ELEMENT_DESC desc[] =
    {
      {"POSITION", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(struct Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(struct Vertex, uv), D3D11_INPUT_PER_VERTEX_DATA, 0},
      {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, offsetof(struct Vertex, color), D3D11_INPUT_PER_VERTEX_DATA, 0},
    };

    char hlsl[] =
    R"FOO(
    
      struct VS_INPUT                                            
      {                                                          
        float2 pos   : POSITION;             
        float2 uv    : TEXCOORD;                              
        float4 color : COLOR;                                 
      };                                                         
                                                                
      struct PS_INPUT                                            
      {                                                          
        float4 pos   : SV_POSITION;             
        float2 uv    : TEXCOORD;                                 
        float4 color : COLOR;                                    
      };                                                         
                                                                
      sampler sampler0 : register(s0);          
                                                                
      Texture2D<float4> texture0 : register(t0);
                                                                
      PS_INPUT vs(VS_INPUT input)                                
      {                                                          
        PS_INPUT output;                                       
        output.pos = float4(input.pos, 0, 1);
        output.uv = input.uv;                                  
        output.color = input.color;
        return output;                                         
      }                                                          
                                                                
      float4 ps(PS_INPUT input) : SV_TARGET                      
      {                                                          
        float4 tex = texture0.Sample(sampler0, input.uv);      
        return input.color * tex;                              
      }

    )FOO";

    UINT flags = D3DCOMPILE_PACK_MATRIX_COLUMN_MAJOR | D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_WARNINGS_ARE_ERRORS;
#ifndef NDEBUG
    flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
    flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

    ID3DBlob *error = 0;

    ID3DBlob *vblob = 0;
    hr = D3DCompile(hlsl, sizeof(hlsl), 0, 0, 0, "vs", "vs_5_0", flags, 0, &vblob, &error);
    if(FAILED(hr))
    {
      const char *message = (const char *)error->GetBufferPointer();
      OutputDebugStringA(message);
      ASSERT(!"Failed to compile vertex shader!");
    }

    ID3DBlob *pblob = 0;
    hr = D3DCompile(hlsl, sizeof(hlsl), 0, 0, 0, "ps", "ps_5_0", flags, 0, &pblob, &error);
    if(FAILED(hr))
    {
      const char *message = (const char *)error->GetBufferPointer();
      OutputDebugStringA(message);
      ASSERT(!"Failed to compile pixel shader!");
    }

    device->CreateVertexShader(vblob->GetBufferPointer(), vblob->GetBufferSize(), 0, &vertex_shader);
    device->CreatePixelShader(pblob->GetBufferPointer(), pblob->GetBufferSize(), 0, &pixel_shader);
    device->CreateInputLayout(desc, ARRAYSIZE(desc), vblob->GetBufferPointer(), vblob->GetBufferSize(), &layout);

    pblob->Release();
    vblob->Release();
  }

  //----------------------------------------------------------
  // hampus: create d3d11 texture atlas

  ID3D11ShaderResourceView *texture_view = 0;
  ID3D11Texture2D *texture = 0;
  {
    D3D11_TEXTURE2D_DESC desc =
    {
      .Width = texture_atlas_width,
      .Height = texture_atlas_height,
      .MipLevels = 1,
      .ArraySize = 1,
      .Format = DXGI_FORMAT_R8G8B8A8_UNORM,
      .SampleDesc = {1, 0},
      .Usage = D3D11_USAGE_DEFAULT,
      .BindFlags = D3D11_BIND_SHADER_RESOURCE,
      .CPUAccessFlags = D3D11_CPU_ACCESS_WRITE,
    };

    D3D11_SUBRESOURCE_DATA data =
    {
      .pSysMem = texture_atlas_data,
      .SysMemPitch = (UINT)(texture_atlas_pitch),
    };

    device->CreateTexture2D(&desc, &data, &texture);
    device->CreateShaderResourceView((ID3D11Resource *)texture, 0, &texture_view);
  }

  //----------------------------------------------------------
  // hampus: create d3d11 sampler state

  ID3D11SamplerState *sampler = 0;
  {
    D3D11_SAMPLER_DESC desc =
    {
      .Filter = D3D11_FILTER_MIN_MAG_MIP_POINT,
      .AddressU = D3D11_TEXTURE_ADDRESS_WRAP,
      .AddressV = D3D11_TEXTURE_ADDRESS_WRAP,
      .AddressW = D3D11_TEXTURE_ADDRESS_WRAP,
      .MipLODBias = 0,
      .MaxAnisotropy = 1,
      .MinLOD = 0,
      .MaxLOD = D3D11_FLOAT32_MAX,
    };

    device->CreateSamplerState(&desc, &sampler);
  }

  //----------------------------------------------------------
  // hampus: create d3d11 blend state

  ID3D11BlendState *blend_state = 0;
  {
    D3D11_BLEND_DESC desc = {};
    desc.RenderTarget[0] =
    {
      .BlendEnable = TRUE,
      .SrcBlend = D3D11_BLEND_SRC_ALPHA,
      .DestBlend = D3D11_BLEND_INV_SRC_ALPHA,
      .BlendOp = D3D11_BLEND_OP_ADD,
      .SrcBlendAlpha = D3D11_BLEND_SRC_ALPHA,
      .DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA,
      .BlendOpAlpha = D3D11_BLEND_OP_ADD,
      .RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL,
    };
    device->CreateBlendState(&desc, &blend_state);
  }

  //----------------------------------------------------------
  // hampus: create d3d11 rasterizer state

  ID3D11RasterizerState *rasterizer_state = 0;
  {
    D3D11_RASTERIZER_DESC desc =
    {
      .FillMode = D3D11_FILL_SOLID,
      .CullMode = D3D11_CULL_NONE,
      .DepthClipEnable = TRUE,
    };
    device->CreateRasterizerState(&desc, &rasterizer_state);
  }

  //----------------------------------------------------------
  // hampus: create d3d11 depth state

  ID3D11DepthStencilState *depth_state = 0;
  {
    D3D11_DEPTH_STENCIL_DESC desc =
    {
      .DepthEnable = FALSE,
      .DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL,
      .DepthFunc = D3D11_COMPARISON_LESS,
      .StencilEnable = FALSE,
      .StencilReadMask = D3D11_DEFAULT_STENCIL_READ_MASK,
      .StencilWriteMask = D3D11_DEFAULT_STENCIL_WRITE_MASK,
    };
    device->CreateDepthStencilState(&desc, &depth_state);
  }

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

  MapTextToGlyphsResult map_text_to_glyphs_result = dwrite_map_text_to_glyphs(font_fallback1, font_collection, text_analyzer1, &locale[0], L"Fira Code", 16.0f, L"Hello->world", wcslen(L"Hello->world"));

  ShowWindow(hwnd, SW_SHOWDEFAULT);

  ID3D11RenderTargetView *render_target_view = 0;
  ID3D11DepthStencilView *depth_stencil_view = 0;

  DWORD current_width = 0;
  DWORD current_height = 0;

  //----------------------------------------------------------
  // hampus: main loop

  g_running = true;
  while(g_running)
  {
    for(MSG message; PeekMessageW(&message, 0, 0, 0, PM_REMOVE);)
    {
      TranslateMessage(&message);
      DispatchMessageW(&message);
    }

    RECT rect = {};
    GetClientRect(hwnd, &rect);
    int width = rect.right - rect.left;
    int height = rect.bottom - rect.top;

    Vec2F32 render_dim = v2f32((float)width, (float)height);

    Vertex vertices[4] = {};
    vertices[0].position = ndc_space_from_pixels_space(render_dim, v2f32(0, 0));
    vertices[0].uv = v2f32(1, 1);
    vertices[0].color = v4f32(1, 1, 1, 1);

    vertices[1].position = ndc_space_from_pixels_space(render_dim, v2f32((float)texture_atlas_width, 0));
    vertices[1].uv = v2f32(1, 0);
    vertices[1].color = v4f32(1, 1, 1, 1);

    vertices[2].position = ndc_space_from_pixels_space(render_dim, v2f32(0, (float)texture_atlas_height));
    vertices[2].uv = v2f32(0, 1);
    vertices[2].color = v4f32(1, 1, 1, 1);

    vertices[3].position = ndc_space_from_pixels_space(render_dim, v2f32((float)texture_atlas_width, (float)texture_atlas_height));
    vertices[3].uv = v2f32(0, 0);
    vertices[3].color = v4f32(1, 1, 1, 1);

    D3D11_MAPPED_SUBRESOURCE mapped = {};
    context->Map((ID3D11Resource *)vertex_buffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
    memcpy((uint8_t *)mapped.pData, (uint8_t *)vertices, sizeof(vertices));
    context->Unmap((ID3D11Resource *)vertex_buffer, 0);

    // hampus: resize render target view if size changed

    if(render_target_view == 0 || width != current_width || height != current_height)
    {
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

      current_width = width;
      current_height = height;
    }

    // hampus: draw

    if(render_target_view)
    {
      D3D11_VIEWPORT viewport =
      {
        .TopLeftX = 0,
        .TopLeftY = 0,
        .Width = (FLOAT)width,
        .Height = (FLOAT)height,
        .MinDepth = 0,
        .MaxDepth = 1,
      };

      // hampus: clear screen

      FLOAT color[] = {0.392f, 0.584f, 0.929f, 1.f};
      context->ClearRenderTargetView(render_target_view, color);
      context->ClearDepthStencilView(depth_stencil_view, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.f, 0);

      // hampus: input assembler

      context->IASetInputLayout(layout);
      context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
      UINT stride = sizeof(Vertex);
      UINT offset = 0;
      context->IASetVertexBuffers(0, 1, &vertex_buffer, &stride, &offset);

      // hampus: vertex shader

      context->VSSetShader(vertex_shader, 0, 0);

      // hampus: rasterizer

      context->RSSetViewports(1, &viewport);
      context->RSSetState(rasterizer_state);

      // hampus: pixel shader

      context->PSSetSamplers(0, 1, &sampler);
      context->PSSetShaderResources(0, 1, &texture_view);
      context->PSSetShader(pixel_shader, 0, 0);

      // hampus: output merger

      context->OMSetBlendState(blend_state, 0, ~0U);
      context->OMSetDepthStencilState(depth_state, 0);
      context->OMSetRenderTargets(1, &render_target_view, depth_stencil_view);

      // hampus: draw 4 vertices

      context->Draw(4, 0);
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

  return 0;
}