#ifndef DWRITE_TEXT_TO_GLYPHS_H
#define DWRITE_TEXT_TO_GLYPHS_H

#include <dwrite_3.h>
#include <stdint.h>

#define ASSERT(expr)        \
  if(!(expr))               \
  {                         \
    *(volatile int *)0 = 0; \
  }

#define ASSERT_HR(hr) ASSERT(SUCCEEDED(hr))

#define memory_copy(dst, src, size) memcpy((uint8_t *)(dst), (uint8_t *)(src), (size))
#define memory_copy_typed(dst, src, count) memcpy((uint8_t *)(dst), (uint8_t *)(src), sizeof(*(dst)) * (count))

struct GlyphArray
{
  GlyphArray *next;
  GlyphArray *prev;

  uint64_t count;
  uint16_t *indices;
  float *advances;
  DWRITE_GLYPH_OFFSET *offsets;
};

struct TextToGlyphsSegment
{
  TextToGlyphsSegment *next;
  TextToGlyphsSegment *prev;

  IDWriteFontFace5 *font_face;
  DWRITE_GLYPH_RUN dwrite_glyph_run;
  GlyphArray glyph_array;
};

struct MapTextToGlyphsResult
{
  // NOTE(hampus): One of these segments for every time the fallback font doesn't match the
  // previous one. For example, if a font contained all the characters, there would just be
  // one segment in the list.

  TextToGlyphsSegment *first_segment;
  TextToGlyphsSegment *last_segment;
};

struct TextAnalysisSource final : IDWriteTextAnalysisSource
{
  TextAnalysisSource(const wchar_t *locale, const wchar_t *text, const UINT32 textLength) noexcept
      : _locale{locale},
        _text{text},
        _text_length{textLength}
  {
  }

  ULONG STDMETHODCALLTYPE
  AddRef() noexcept override
  {
    return 1;
  }

  ULONG STDMETHODCALLTYPE
  Release() noexcept override
  {
    return 1;
  }

  HRESULT STDMETHODCALLTYPE
  QueryInterface(const IID &riid, void **ppvObject) noexcept override
  {
    if(IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSource)))
    {
      *ppvObject = this;
      return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE
  GetTextAtPosition(UINT32 textPosition, const WCHAR **textString, UINT32 *textLength) noexcept override
  {
    textPosition = min(textPosition, _text_length);
    *textString = _text + textPosition;
    *textLength = _text_length - textPosition;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetTextBeforePosition(UINT32 textPosition, const WCHAR **textString, UINT32 *textLength) noexcept override
  {
    textPosition = min(textPosition, _text_length);
    *textString = _text;
    *textLength = textPosition;
    return S_OK;
  }

  DWRITE_READING_DIRECTION STDMETHODCALLTYPE
  GetParagraphReadingDirection() noexcept override
  {
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
  }

  HRESULT STDMETHODCALLTYPE
  GetLocaleName(UINT32 textPosition, UINT32 *textLength, const WCHAR **localeName) noexcept override
  {
    *textLength = _text_length - textPosition;
    *localeName = _locale;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetNumberSubstitution(UINT32 textPosition, UINT32 *textLength, IDWriteNumberSubstitution **numberSubstitution) noexcept override
  {
    return E_NOTIMPL;
  }

private:
  const wchar_t *_locale;
  const wchar_t *_text;
  const UINT32 _text_length;
};

struct TextAnalysisSinkResult
{
  uint32_t text_position;
  uint32_t text_length;
  DWRITE_SCRIPT_ANALYSIS analysis;
};

struct TextAnalysisSinkResultChunk
{
  TextAnalysisSinkResultChunk *next;
  TextAnalysisSinkResultChunk *prev;
  uint64_t count;
  TextAnalysisSinkResult v[512];
};

// DirectWrite uses an IDWriteTextAnalysisSink to inform the caller of its segmentation results. The most important part are the
// DWRITE_SCRIPT_ANALYSIS results which inform the remaining steps during glyph shaping what script ("language") is used in a piece of text.
struct TextAnalysisSink final : IDWriteTextAnalysisSink
{
  TextAnalysisSinkResultChunk *first_result_chunk;
  TextAnalysisSinkResultChunk *last_result_chunk;

  ULONG STDMETHODCALLTYPE
  AddRef() noexcept override
  {
    return 1;
  }

  ULONG STDMETHODCALLTYPE
  Release() noexcept override
  {
    return 1;
  }

  HRESULT STDMETHODCALLTYPE
  QueryInterface(const IID &riid, void **ppvObject) noexcept override
  {
    if(IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink)))
    {
      *ppvObject = this;
      return S_OK;
    }

    *ppvObject = nullptr;
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE
  SetScriptAnalysis(UINT32 textPosition, UINT32 textLength, const DWRITE_SCRIPT_ANALYSIS *scriptAnalysis) noexcept override
  {
    TextAnalysisSinkResultChunk *chunk = last_result_chunk;
    if(chunk == 0 || chunk->count == ARRAYSIZE(chunk->v))
    {
      chunk = (TextAnalysisSinkResultChunk *)calloc(1, sizeof(TextAnalysisSinkResultChunk));
      if(first_result_chunk == 0)
      {
        first_result_chunk = last_result_chunk = chunk;
      }
      else
      {
        last_result_chunk->next = chunk;
        last_result_chunk = first_result_chunk;
      }
    }
    TextAnalysisSinkResult &result = chunk->v[chunk->count];
    result.text_position = textPosition;
    result.text_length = textLength;
    result.analysis = *scriptAnalysis;
    chunk->count += 1;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetLineBreakpoints(UINT32 textPosition, UINT32 textLength, const DWRITE_LINE_BREAKPOINT *lineBreakpoints) noexcept override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  SetBidiLevel(UINT32 textPosition, UINT32 textLength, UINT8 explicitLevel, UINT8 resolvedLevel) noexcept override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  SetNumberSubstitution(UINT32 textPosition, UINT32 textLength, IDWriteNumberSubstitution *numberSubstitution) noexcept override
  {
    return E_NOTIMPL;
  }

  ~TextAnalysisSink()
  {
    TextAnalysisSinkResultChunk *next_chunk = 0;
    for(TextAnalysisSinkResultChunk *chunk = first_result_chunk; chunk != 0; chunk = next_chunk)
    {
      next_chunk = chunk->next;
      free(chunk);
    }
  }
};

static MapTextToGlyphsResult
dwrite_map_text_to_glyphs(IDWriteFontFallback1 *font_fallback, IDWriteFontCollection *font_collection, IDWriteTextAnalyzer1 *text_analyzer, const wchar_t *locale, const wchar_t *base_family, float font_size, const wchar_t *text, uint32_t text_length)
{
  MapTextToGlyphsResult result = {};

  HRESULT hr = 0;

  for(uint32_t fallback_offset = 0; fallback_offset < text_length;)
  {
    //----------------------------------------------------------
    // hampus: get mapped font and length

    IDWriteFontFace5 *fallback_font_face = 0;
    uint32_t fallback_text_length = 0;
    {
      float scale = 0;
      TextAnalysisSource analysis_source{locale, text, text_length};
      hr = font_fallback->MapCharacters(&analysis_source,
                                        fallback_offset,
                                        text_length,
                                        font_collection,
                                        base_family,
                                        0,
                                        0,
                                        &fallback_text_length,
                                        &scale,
                                        &fallback_font_face);
      ASSERT_HR(hr);
      if(fallback_font_face == 0)
      {
        // NOTE(hampus): This means that no font was available for this character.
        // TODO(hampus): Should be replaced by '?'
        continue;
      }
    }

    //----------------------------------------------------------
    // hampus: new fallback font => new segment

    TextToGlyphsSegment *segment = (TextToGlyphsSegment *)calloc(1, sizeof(TextToGlyphsSegment));
    if(result.first_segment == 0)
    {
      result.first_segment = result.last_segment = segment;
    }
    else
    {
      result.last_segment->next = segment;
      result.last_segment = segment;
    }

    //----------------------------------------------------------
    // hampus: get glyph array list with both simple and complex glyphs

    // NOTE(hampus): Each simple and complex text will get their own GlyphArray.
    // In the end a long contigous array will be allocated and the glyph runs
    // will be memcpy'd into that. That is because DWRITE_GLYPH_RUN expcets
    // one large array of glyph indices. So these chunks are only temporary.

    struct GlyphArrayChunk
    {
      GlyphArrayChunk *next;
      GlyphArrayChunk *prev;
      uint64_t glyph_array_count;
      uint64_t glyph_count;
      GlyphArray v[512];
    };

    GlyphArrayChunk *first_glyph_array_chunk = 0;
    GlyphArrayChunk *last_glyph_array_chunk = 0;

    const wchar_t *fallback_ptr = text + fallback_offset;
    const wchar_t *fallback_opl = fallback_ptr + fallback_text_length;
    while(fallback_ptr < fallback_opl)
    {
      uint32_t fallback_remaining = (uint32_t)(fallback_opl - fallback_ptr);

      uint16_t *glyph_indices = (uint16_t *)calloc(fallback_remaining, sizeof(uint16_t));
      BOOL is_simple = FALSE;
      uint32_t complex_mapped_length = 0;

      hr = text_analyzer->GetTextComplexity(fallback_ptr,
                                            fallback_remaining,
                                            fallback_font_face,
                                            &is_simple,
                                            &complex_mapped_length,
                                            glyph_indices);
      ASSERT_HR(hr);

      // hampus: get a new glyph array

      GlyphArray *glyph_array = 0;
      GlyphArrayChunk *glyph_array_chunk = last_glyph_array_chunk;
      {
        if(glyph_array_chunk == 0 || glyph_array_chunk->glyph_array_count == ARRAYSIZE(glyph_array_chunk->v))
        {
          glyph_array_chunk = (GlyphArrayChunk *)calloc(1, sizeof(GlyphArrayChunk));
          if(first_glyph_array_chunk == 0)
          {
            first_glyph_array_chunk = last_glyph_array_chunk = glyph_array_chunk;
          }
          else
          {
            last_glyph_array_chunk->next = glyph_array_chunk;
            last_glyph_array_chunk = glyph_array_chunk;
          }
        }
        glyph_array = &glyph_array_chunk->v[glyph_array_chunk->glyph_array_count];
        glyph_array_chunk->glyph_array_count += 1;
      }

      // hampus: fill in the glyph array

      if(is_simple)
      {
        // NOTE(hampus): This text was simple. This means we can just use
        // the indices directly without having to do any more shaping work.

        // hampus: allocate arrays

        DWRITE_FONT_METRICS1 font_metrics = {};
        fallback_font_face->GetMetrics(&font_metrics);
        glyph_array->count = complex_mapped_length;
        glyph_array->advances = (float *)calloc(glyph_array->count, sizeof(float));
        glyph_array->offsets = (DWRITE_GLYPH_OFFSET *)calloc(glyph_array->count, sizeof(DWRITE_GLYPH_OFFSET));
        glyph_array->indices = (uint16_t *)calloc(glyph_array->count, sizeof(uint16_t));

        // hampus: fill in indices

        memory_copy_typed(glyph_array->indices, glyph_indices, glyph_array->count);

        // hampus: fill in advances

        {
          int32_t *design_advances = (int32_t *)calloc(glyph_array->count, sizeof(int32_t));
          hr = fallback_font_face->GetDesignGlyphAdvances(glyph_array->count, glyph_array->indices, design_advances);
          float scale = font_size / (float)font_metrics.designUnitsPerEm;
          for(uint64_t idx = 0; idx < glyph_array->count; idx++)
          {
            glyph_array->advances[idx] = (float)design_advances[idx] * scale;
          }
          free(design_advances);
        }

        glyph_array_chunk->glyph_count += glyph_array->count;
      }
      else
      {
        // NOTE(hampus): This text was not simple. We have to do extra work. :(

        TextAnalysisSource analysis_source{locale, text, text_length};
        TextAnalysisSink analysis_sink = {};
      }

      free(glyph_indices);

      fallback_ptr += complex_mapped_length;
    }

    //----------------------------------------------------------
    // hampus: convert our list of glyph arrays into one big array

    {
      uint64_t total_glyph_count = 0;
      for(GlyphArrayChunk *chunk = first_glyph_array_chunk; chunk != 0; chunk = chunk->next)
      {
        total_glyph_count += chunk->glyph_count;
      }

      GlyphArray &segment_glyph_array = segment->glyph_array;
      segment_glyph_array.count = total_glyph_count;
      segment_glyph_array.indices = (uint16_t *)calloc(segment_glyph_array.count, sizeof(uint16_t));
      segment_glyph_array.advances = (float *)calloc(segment_glyph_array.count, sizeof(float));
      segment_glyph_array.offsets = (DWRITE_GLYPH_OFFSET *)calloc(segment_glyph_array.count, sizeof(DWRITE_GLYPH_OFFSET));
      uint64_t glyph_idx_offset = 0;
      GlyphArrayChunk *next_chunk = 0;
      for(GlyphArrayChunk *chunk = first_glyph_array_chunk; chunk != 0; chunk = next_chunk)
      {
        next_chunk = chunk->next;
        for(uint64_t glyph_array_idx = 0; glyph_array_idx < chunk->glyph_array_count; ++glyph_array_idx)
        {
          GlyphArray &glyph_array = chunk->v[glyph_array_idx];
          memory_copy_typed(segment_glyph_array.indices + glyph_idx_offset, glyph_array.indices, glyph_array.count);
          memory_copy_typed(segment_glyph_array.advances + glyph_idx_offset, glyph_array.advances, glyph_array.count);
          memory_copy_typed(segment_glyph_array.offsets + glyph_idx_offset, glyph_array.offsets, glyph_array.count);

          free(glyph_array.indices);
          free(glyph_array.advances);
          free(glyph_array.offsets);

          glyph_idx_offset += glyph_array.count;
        }
        free(chunk);
      }
    }

    segment->dwrite_glyph_run.fontFace = fallback_font_face;
    segment->dwrite_glyph_run.fontEmSize = font_size;
    segment->dwrite_glyph_run.glyphCount = segment->glyph_array.count;
    segment->dwrite_glyph_run.glyphAdvances = segment->glyph_array.advances;
    segment->dwrite_glyph_run.glyphIndices = segment->glyph_array.indices;
    segment->dwrite_glyph_run.glyphOffsets = segment->glyph_array.offsets;

    fallback_offset += fallback_text_length;
  }

  return result;
}

#endif // DWRITE_TEXT_TO_GLYPHS_H