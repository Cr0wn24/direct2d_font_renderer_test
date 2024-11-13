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

struct GlyphArrayChunk;

struct GlyphArray
{
  uint64_t count;
  uint16_t *indices;
  float *advances;
  DWRITE_GLYPH_OFFSET *offsets;
};

struct GlyphArrayChunk
{
  GlyphArrayChunk *next;
  GlyphArrayChunk *prev;
  uint64_t total_glyph_count;
  uint64_t count;
  GlyphArray v[512];
};

struct TextToGlyphsSegment
{
  // per segment data
  IDWriteFontFace5 *font_face;
  uint32_t bidi_level;
  FLOAT font_size_em;
  uint64_t glyph_count;

  // per glyph data
  uint16_t *glyph_indices;
  float *glyph_advances;
  DWRITE_GLYPH_OFFSET *glyph_offsets;
};

struct TextToGlyphsSegmentNode
{
  TextToGlyphsSegmentNode *next;
  TextToGlyphsSegmentNode *prev;
  TextToGlyphsSegment v;
};

struct MapTextToGlyphsResult
{
  // NOTE(hampus): One of these segments for every time the fallback font doesn't match the
  // previous one. For example, if a font contained all the characters, there would just be
  // one segment in the list.

  TextToGlyphsSegmentNode *first_segment;
  TextToGlyphsSegmentNode *last_segment;
};

struct TextAnalysisSource final : IDWriteTextAnalysisSource
{
  TextAnalysisSource(const wchar_t *locale, const wchar_t *text, const UINT32 text_length) noexcept
      : _locale{locale},
        _text{text},
        _text_length{text_length}
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

    *ppvObject = 0;
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE
  GetTextAtPosition(UINT32 text_pos, const WCHAR **text_string, UINT32 *text_length) noexcept override
  {
    text_pos = min(text_pos, _text_length);
    *text_string = _text + text_pos;
    *text_length = _text_length - text_pos;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetTextBeforePosition(UINT32 text_pos, const WCHAR **text_string, UINT32 *text_length) noexcept override
  {
    text_pos = min(text_pos, _text_length);
    *text_string = _text;
    *text_length = text_pos;
    return S_OK;
  }

  DWRITE_READING_DIRECTION STDMETHODCALLTYPE
  GetParagraphReadingDirection() noexcept override
  {
    return DWRITE_READING_DIRECTION_LEFT_TO_RIGHT;
  }

  HRESULT STDMETHODCALLTYPE
  GetLocaleName(UINT32 text_pos, UINT32 *text_length, const WCHAR **locale_name) noexcept override
  {
    *text_length = _text_length - text_pos;
    *locale_name = _locale;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  GetNumberSubstitution(UINT32 text_pos, UINT32 *text_length, IDWriteNumberSubstitution **nunber_substitution) noexcept override
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
  uint32_t resolved_bidi_level;
  uint32_t explicit_bidi_level;
};

struct TextAnalysisSinkResultChunk
{
  TextAnalysisSinkResultChunk *next;
  TextAnalysisSinkResultChunk *prev;
  uint64_t count;
  TextAnalysisSinkResult v[512];
};

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
  QueryInterface(const IID &riid, void **object) noexcept override
  {
    if(IsEqualGUID(riid, __uuidof(IDWriteTextAnalysisSink)))
    {
      *object = this;
      return S_OK;
    }

    object = 0;
    return E_NOINTERFACE;
  }

  HRESULT STDMETHODCALLTYPE
  SetScriptAnalysis(UINT32 text_pos, UINT32 text_length, const DWRITE_SCRIPT_ANALYSIS *script_analysis) noexcept override
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
    result.text_position = text_pos;
    result.text_length = text_length;
    result.analysis = *script_analysis;
    chunk->count += 1;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetLineBreakpoints(UINT32 text_pos, UINT32 text_length, const DWRITE_LINE_BREAKPOINT *line_breakpoints) noexcept override
  {
    return E_NOTIMPL;
  }

  HRESULT STDMETHODCALLTYPE
  SetBidiLevel(UINT32 text_pos, UINT32 text_length, UINT8 explicit_level, UINT8 resolved_level) noexcept override
  {
    // TODO(hampus): Is this correct?
    last_result_chunk->v[last_result_chunk->count - 1].explicit_bidi_level = explicit_level;
    last_result_chunk->v[last_result_chunk->count - 1].resolved_bidi_level = resolved_level;
    return S_OK;
  }

  HRESULT STDMETHODCALLTYPE
  SetNumberSubstitution(UINT32 text_pos, UINT32 text_length, IDWriteNumberSubstitution *nunber_substitution) noexcept override
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

static GlyphArray *
allocate_and_push_back_glyph_array(GlyphArrayChunk **first_chunk, GlyphArrayChunk **last_chunk)
{
  GlyphArray *glyph_array = 0;
  GlyphArrayChunk *glyph_array_chunk = *last_chunk;
  {
    if(glyph_array_chunk == 0 || glyph_array_chunk->count == ARRAYSIZE(glyph_array_chunk->v))
    {
      glyph_array_chunk = (GlyphArrayChunk *)calloc(1, sizeof(GlyphArrayChunk));
      if(*first_chunk == 0)
      {
        *first_chunk = *last_chunk = glyph_array_chunk;
      }
      else
      {
        (*last_chunk)->next = glyph_array_chunk;
        glyph_array_chunk->prev = *last_chunk;
        *last_chunk = glyph_array_chunk;
      }
    }
    glyph_array = &glyph_array_chunk->v[glyph_array_chunk->count];
    glyph_array_chunk->count += 1;
  }
  return glyph_array;
}

static TextToGlyphsSegmentNode *
allocate_and_push_back_segment_node(TextToGlyphsSegmentNode **first_segment, TextToGlyphsSegmentNode **last_segment)
{
  TextToGlyphsSegmentNode *segment_node = (TextToGlyphsSegmentNode *)calloc(1, sizeof(TextToGlyphsSegmentNode));
  if(*first_segment == 0)
  {
    *first_segment = *last_segment = segment_node;
  }
  else
  {
    (*last_segment)->next = segment_node;
    segment_node->prev = *last_segment;
    *last_segment = segment_node;
  }
  return segment_node;
}

static void
fill_segment_with_glyph_array_chunks(TextToGlyphsSegment *segment, GlyphArrayChunk *first_chunk, GlyphArrayChunk *last_chunk)
{
  uint64_t total_glyph_count = 0;
  for(GlyphArrayChunk *chunk = first_chunk; chunk != 0; chunk = chunk->next)
  {
    total_glyph_count += chunk->total_glyph_count;
  }

  segment->glyph_count = total_glyph_count;
  segment->glyph_indices = (uint16_t *)calloc(segment->glyph_count, sizeof(uint16_t));
  segment->glyph_advances = (float *)calloc(segment->glyph_count, sizeof(float));
  segment->glyph_offsets = (DWRITE_GLYPH_OFFSET *)calloc(segment->glyph_count, sizeof(DWRITE_GLYPH_OFFSET));
  uint64_t glyph_idx_offset = 0;
  GlyphArrayChunk *next_chunk = 0;
  for(GlyphArrayChunk *chunk = first_chunk; chunk != 0; chunk = next_chunk)
  {
    next_chunk = chunk->next;
    for(uint64_t glyph_array_idx = 0; glyph_array_idx < chunk->count; ++glyph_array_idx)
    {
      GlyphArray &glyph_array = chunk->v[glyph_array_idx];
      memory_copy_typed(segment->glyph_indices + glyph_idx_offset, glyph_array.indices, glyph_array.count);
      memory_copy_typed(segment->glyph_advances + glyph_idx_offset, glyph_array.advances, glyph_array.count);
      memory_copy_typed(segment->glyph_offsets + glyph_idx_offset, glyph_array.offsets, glyph_array.count);

      free(glyph_array.indices);
      free(glyph_array.advances);
      free(glyph_array.offsets);

      glyph_idx_offset += glyph_array.count;
    }
    free(chunk);
  }
}

static MapTextToGlyphsResult
dwrite_map_text_to_glyphs(IDWriteFontFallback1 *font_fallback, IDWriteFontCollection *font_collection, IDWriteTextAnalyzer1 *text_analyzer, const wchar_t *locale, const wchar_t *base_family, const float font_size, const wchar_t *text, const uint32_t text_length)
{
  MapTextToGlyphsResult result = {};

  HRESULT hr = 0;

  struct MappedText
  {
    MappedText *next;
    MappedText *prev;

    IDWriteFontFace5 *font_face;
    uint32_t text_offset;
    uint32_t text_length;
  };

  MappedText *first_mapping = 0;
  MappedText *last_mapping = 0;

  for(uint32_t fallback_offset = 0; fallback_offset < text_length;)
  {
    MappedText *mapping = (MappedText *)calloc(1, sizeof(MappedText));

    //----------------------------------------------------------
    // hampus: get mapped font and length

    IDWriteFontFace5 *mapped_font_face = 0;
    uint32_t mapped_text_length = 0;
    {
      // NOTE(hampus): We need an analysis source that holds the text and the locale
      TextAnalysisSource analysis_source{locale, text+fallback_offset, text_length-fallback_offset};

      // NOTE(hampus): This get the appropiate font required for rendering the text
      float scale = 0;
      hr = font_fallback->MapCharacters(&analysis_source,
                                        0,
                                        text_length-fallback_offset,
                                        font_collection,
                                        base_family,
                                        0,
                                        0,
                                        &mapped_text_length,
                                        &scale,
                                        &mapped_font_face);
      ASSERT_HR(hr);
      if(mapped_font_face == 0)
      {
        // NOTE(hampus): This means that no font was available for this character.
        // mapped_text_length is the number of characters to skip.
        // Should be replaced by a missing glyph, typically glyph index 0 in fonts.
      }
    }

    mapping->text_offset = fallback_offset;
    mapping->text_length = mapped_text_length;
    mapping->font_face = mapped_font_face;

    BOOL insert = TRUE;
    if(last_mapping != 0)
    {
      if(last_mapping->font_face == mapped_font_face)
      {
        last_mapping->text_length += mapping->text_length;
        insert = FALSE;
      }
    }

    if(insert)
    {
      if(first_mapping == 0)
      {
        first_mapping = last_mapping = mapping;
      }
      else
      {
        mapping->prev = last_mapping;
        last_mapping->next = mapping;
        last_mapping = mapping;
      }
    }
    else
    {
      free(mapping);
      mapping = 0;
    }

    fallback_offset += mapped_text_length;
  }

  for(MappedText *mapping = first_mapping; mapping != 0; mapping = mapping->next)
  {
    if(mapping->font_face == 0)
    {
      continue;
    }

    TextToGlyphsSegment *segment = 0;
    GlyphArrayChunk *first_glyph_array_chunk = 0;
    GlyphArrayChunk *last_glyph_array_chunk = 0;

    //----------------------------------------------------------
    // hampus: get glyph array list with both simple and complex glyphs

    // NOTE(hampus): Each simple and complex text will get their own GlyphArray.
    // In the end a long contigous array will be allocated and the glyph runs
    // will be memcpy'd into that. That is because DWRITE_GLYPH_RUN expcets
    // one large array of glyph indices. So these chunks are only temporary.

    // TODO(hampus): Is chunking really necessary? Since this memory is just temporary and
    // if we know the upper limits, we could just preallocate GlyphArray and not having to deal with
    // chunks.

    const wchar_t *fallback_ptr = text + mapping->text_offset;
    const wchar_t *fallback_opl = fallback_ptr + mapping->text_length;
    while(fallback_ptr < fallback_opl)
    {
      uint32_t fallback_remaining = (uint32_t)(fallback_opl - fallback_ptr);

      uint64_t max_glyph_indices_count = (3 * mapping->text_length) / 2 + 16;
      uint16_t *glyph_indices = (uint16_t *)calloc(max_glyph_indices_count, sizeof(uint16_t));
      BOOL is_simple = FALSE;
      uint32_t complex_mapped_length = 0;

      hr = text_analyzer->GetTextComplexity(fallback_ptr,
                                            fallback_remaining,
                                            mapping->font_face,
                                            &is_simple,
                                            &complex_mapped_length,
                                            glyph_indices);
      ASSERT_HR(hr);

      if(is_simple)
      {
        // NOTE(hampus): This text was simple. This means we can just use
        // the indices directly without having to do any more shaping work.

        if(segment != 0)
        {
          if(segment->bidi_level != 0)
          {
            fill_segment_with_glyph_array_chunks(segment, first_glyph_array_chunk, last_glyph_array_chunk);
            first_glyph_array_chunk = 0;
            last_glyph_array_chunk = 0;
            segment = 0;
          }
        }

        if(segment == 0)
        {
          TextToGlyphsSegmentNode *segment_node = allocate_and_push_back_segment_node(&result.first_segment, &result.last_segment);
          segment = &segment_node->v;
          segment->font_face = mapping->font_face;
          segment->font_size_em = font_size;
        }

        // hampus: get a new glyph array

        GlyphArray *glyph_array = allocate_and_push_back_glyph_array(&first_glyph_array_chunk, &last_glyph_array_chunk);

        // hampus: allocate the arrays in the glyph array

        DWRITE_FONT_METRICS1 font_metrics = {};
        mapping->font_face->GetMetrics(&font_metrics);
        glyph_array->count = complex_mapped_length;
        glyph_array->advances = (float *)calloc(glyph_array->count, sizeof(float));
        glyph_array->offsets = (DWRITE_GLYPH_OFFSET *)calloc(glyph_array->count, sizeof(DWRITE_GLYPH_OFFSET));
        glyph_array->indices = (uint16_t *)calloc(glyph_array->count, sizeof(uint16_t));

        // hampus: fill in indices

        memory_copy_typed(glyph_array->indices, glyph_indices, glyph_array->count);

        // hampus: fill in advances

        {
          int32_t *design_advances = (int32_t *)calloc(glyph_array->count, sizeof(int32_t));
          hr = mapping->font_face->GetDesignGlyphAdvances(glyph_array->count, glyph_array->indices, design_advances);
          float scale = font_size / (float)font_metrics.designUnitsPerEm;
          for(uint64_t idx = 0; idx < glyph_array->count; idx++)
          {
            glyph_array->advances[idx] = (float)design_advances[idx] * scale;
          }
          free(design_advances);
        }

        last_glyph_array_chunk->total_glyph_count += glyph_array->count;
      }
      else
      {

        // NOTE(hampus): This text was not simple. We have to do extra work. :(

        TextAnalysisSource analysis_source{locale, fallback_ptr, complex_mapped_length};
        TextAnalysisSink analysis_sink = {};

        hr = text_analyzer->AnalyzeScript(&analysis_source, 0, complex_mapped_length, &analysis_sink);
        ASSERT_HR(hr);

        // NOTE(hampus): The text range given to AnalyzeBidi should not split a paragraph.
        // It is meant for one paragraph as a whole or multiple paragraphs.
        // TODO(hampus): What to do about it? What is a paragraph?
        hr = text_analyzer->AnalyzeBidi(&analysis_source, 0, complex_mapped_length, &analysis_sink);
        ASSERT_HR(hr);

        DWRITE_SHAPING_GLYPH_PROPERTIES *glyph_props = (DWRITE_SHAPING_GLYPH_PROPERTIES *)calloc(max_glyph_indices_count, sizeof(DWRITE_SHAPING_GLYPH_PROPERTIES));

        for(TextAnalysisSinkResultChunk *chunk = analysis_sink.first_result_chunk; chunk != 0; chunk = chunk->next)
        {
          for(uint64_t text_analysis_sink_result_idx = 0; text_analysis_sink_result_idx < chunk->count; ++text_analysis_sink_result_idx)
          {
            TextAnalysisSinkResult &analysis_result = chunk->v[text_analysis_sink_result_idx];

            if(segment != 0)
            {
              if(segment->bidi_level != analysis_result.resolved_bidi_level)
              {
                fill_segment_with_glyph_array_chunks(segment, first_glyph_array_chunk, last_glyph_array_chunk);
                first_glyph_array_chunk = 0;
                last_glyph_array_chunk = 0;
                segment = 0;
              }
            }

            if(segment == 0)
            {
              TextToGlyphsSegmentNode *segment_node = allocate_and_push_back_segment_node(&result.first_segment, &result.last_segment);
              segment = &segment_node->v;
              segment->font_face = mapping->font_face;
              segment->font_size_em = font_size;
              segment->bidi_level = analysis_result.resolved_bidi_level;
            }

            uint16_t *cluster_map = (uint16_t *)calloc(analysis_result.text_length, sizeof(uint16_t));
            DWRITE_SHAPING_TEXT_PROPERTIES *text_props = (DWRITE_SHAPING_TEXT_PROPERTIES *)calloc(analysis_result.text_length, sizeof(DWRITE_SHAPING_TEXT_PROPERTIES));

            uint32_t actual_glyph_count = 0;

            GlyphArray *glyph_array = allocate_and_push_back_glyph_array(&first_glyph_array_chunk, &last_glyph_array_chunk);

            glyph_array->indices = (uint16_t *)calloc(max_glyph_indices_count, sizeof(uint16_t));
            BOOL is_right_to_left = (BOOL)(analysis_result.resolved_bidi_level & 1);
            for(int retry = 0;;)
            {
              hr = text_analyzer->GetGlyphs(fallback_ptr + analysis_result.text_position,
                                            analysis_result.text_length,
                                            mapping->font_face,
                                            false,
                                            is_right_to_left,
                                            &analysis_result.analysis,
                                            locale,
                                            0,
                                            0,
                                            0,
                                            0,
                                            max_glyph_indices_count,
                                            cluster_map,
                                            text_props,
                                            glyph_indices,
                                            glyph_props,
                                            &actual_glyph_count);

              if(hr == HRESULT_FROM_WIN32(ERROR_INSUFFICIENT_BUFFER) && ++retry < 8)
              {
                // TODO(hampus): Test this codepath.
                max_glyph_indices_count *= 2;
                glyph_indices = (uint16_t *)realloc(glyph_indices, max_glyph_indices_count * sizeof(uint16_t));
                glyph_props = (DWRITE_SHAPING_GLYPH_PROPERTIES *)realloc(glyph_props, max_glyph_indices_count * sizeof(DWRITE_SHAPING_GLYPH_PROPERTIES));
              }

              ASSERT_HR(hr);
              break;
            }

            glyph_array->count = actual_glyph_count;
            glyph_array->indices = (uint16_t *)calloc(glyph_array->count, sizeof(uint16_t));
            glyph_array->advances = (float *)calloc(glyph_array->count, sizeof(float));
            glyph_array->offsets = (DWRITE_GLYPH_OFFSET *)calloc(glyph_array->count, sizeof(DWRITE_GLYPH_OFFSET));

            memory_copy_typed(glyph_array->indices, glyph_indices, actual_glyph_count);

            hr = text_analyzer->GetGlyphPlacements(fallback_ptr + analysis_result.text_position,
                                                   cluster_map,
                                                   text_props,
                                                   analysis_result.text_length,
                                                   glyph_array->indices,
                                                   glyph_props,
                                                   actual_glyph_count,
                                                   mapping->font_face,
                                                   font_size,
                                                   false,
                                                   is_right_to_left,
                                                   &analysis_result.analysis,
                                                   locale,
                                                   0,
                                                   0,
                                                   0,
                                                   glyph_array->advances,
                                                   glyph_array->offsets);
            ASSERT_HR(hr);

            free(text_props);
            free(cluster_map);

            last_glyph_array_chunk->total_glyph_count += glyph_array->count;
          }
        }

        free(glyph_props);
      }

      free(glyph_indices);

      fallback_ptr += complex_mapped_length;
    }

    //----------------------------------------------------------
    // hampus: convert our list of glyph arrays into one big array

    fill_segment_with_glyph_array_chunks(segment, first_glyph_array_chunk, last_glyph_array_chunk);
  }

  {
    MappedText *next_mapping = 0;
    for(MappedText *mapping = first_mapping; mapping != 0; mapping = next_mapping)
    {
      next_mapping = mapping->next;
      free(mapping);
    }
  }
  return result;
}

#endif // DWRITE_TEXT_TO_GLYPHS_H