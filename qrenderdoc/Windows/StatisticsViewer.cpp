/******************************************************************************
 * The MIT License (MIT)
 *
 * Copyright (c) 2017 Baldur Karlsson
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 ******************************************************************************/

#include "StatisticsViewer.h"
#include <QFontDatabase>
#include "ui_StatisticsViewer.h"

static const int HistogramWidth = 128;
static const QString Stars = QString(HistogramWidth, QChar('*'));

QString Pow2IndexAsReadable(int index)
{
  uint64_t value = 1ULL << index;

  if(value >= (1024 * 1024))
  {
    float slice = (float)value / (1024 * 1024);
    return QString("%1MB").arg(Formatter::Format(slice));
  }
  else if(value >= 1024)
  {
    float slice = (float)value / 1024;
    return QString("%1KB").arg(Formatter::Format(slice));
  }
  else
  {
    return QString("%1B").arg(Formatter::Format((float)value));
  }
}

int SliceForString(const QString &s, uint32_t value, uint32_t maximum)
{
  if(value == 0 || maximum == 0)
    return 0;

  float ratio = (float)value / maximum;
  int slice = (int)(ratio * s.length());
  return qMax(1, slice);
}

QString CountOrEmpty(uint32_t count)
{
  if(count == 0)
    return "";
  else
    return QString("(%1)").arg(count);
}

QString CreateSimpleIntegerHistogram(const QString &legend, const rdctype::array<uint32_t> &array)
{
  uint32_t maxCount = 0;
  int maxWithValue = 0;

  for(int o = 0; o < array.count; o++)
  {
    uint32_t value = array[o];
    if(value > 0)
      maxWithValue = o;
    maxCount = qMax(maxCount, value);
  }

  QString text = QString("\n%1:\n").arg(legend);

  for(int o = 0; o <= maxWithValue; o++)
  {
    uint32_t count = array[o];
    int slice = SliceForString(Stars, count, maxCount);
    text += QString("%1: %2 %3\n").arg(o, 4).arg(Stars.left(slice)).arg(CountOrEmpty(count));
  }

  return text;
}

void AppendDrawStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  // #mivance see AppendConstantBindStatistics
  const DrawcallStats &draws = frameInfo.stats.draws;

  statisticsLog.append("\n*** Draw Statistics ***\n\n");

  statisticsLog.append(QString("Total calls: %1, instanced: %2, indirect: %3\n")
                           .arg(draws.calls)
                           .arg(draws.instanced)
                           .arg(draws.indirect));

  if(draws.instanced > 0)
  {
    statisticsLog.append("\nInstance counts:\n");
    uint32_t maxCount = 0;
    int maxWithValue = 0;
    int maximum = draws.counts.count;
    for(int s = 1; s < maximum; s++)
    {
      uint32_t value = draws.counts[s];
      if(value > 0)
        maxWithValue = s;
      maxCount = qMax(maxCount, value);
    }

    for(int s = 1; s <= maxWithValue; s++)
    {
      uint32_t count = draws.counts[s];
      int slice = SliceForString(Stars, count, maxCount);
      statisticsLog.append(QString("%1%2: %3 %4\n")
                               .arg((s == maximum - 1) ? ">=" : "  ")
                               .arg(s, 2)
                               .arg(Stars.left(slice))
                               .arg(CountOrEmpty(count)));
    }
  }
}

void AppendDispatchStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  statisticsLog.append("\n*** Dispatch Statistics ***\n\n");
  statisticsLog.append(QString("Total calls: %1, indirect: %2\n")
                           .arg(frameInfo.stats.dispatches.calls)
                           .arg(frameInfo.stats.dispatches.indirect));
}

void AppendInputAssemblerStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  const IndexBindStats &indices = frameInfo.stats.indices;
  const LayoutBindStats &layouts = frameInfo.stats.layouts;

  const VertexBindStats &vertices = frameInfo.stats.vertices;

  statisticsLog.append("\n*** Input Assembler Statistics ***\n\n");

  statisticsLog.append(
      QString("Total index calls: %1, non-null index sets: %2, null index sets: %3\n")
          .arg(indices.calls)
          .arg(indices.sets)
          .arg(indices.nulls));
  statisticsLog.append(
      QString("Total layout calls: %1, non-null layout sets: %2, null layout sets: %3\n")
          .arg(layouts.calls)
          .arg(layouts.sets)
          .arg(layouts.nulls));
  statisticsLog.append(
      QString("Total vertex calls: %1, non-null vertex sets: %2, null vertex sets: %3\n")
          .arg(vertices.calls)
          .arg(vertices.sets)
          .arg(vertices.nulls));

  statisticsLog.append(CreateSimpleIntegerHistogram("Aggregate vertex slot counts per invocation",
                                                    vertices.bindslots));
}

void AppendShaderStatistics(ICaptureContext &ctx, QString &statisticsLog,
                            const FrameDescription &frameInfo)
{
  const ShaderChangeStats *shaders = frameInfo.stats.shaders;
  ShaderChangeStats totalShadersPerStage;
  memset(&totalShadersPerStage, 0, sizeof(totalShadersPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalShadersPerStage.calls += shaders[s].calls;
    totalShadersPerStage.sets += shaders[s].sets;
    totalShadersPerStage.nulls += shaders[s].nulls;
    totalShadersPerStage.redundants += shaders[s].redundants;
  }

  statisticsLog.append("\n*** Shader Set Statistics ***\n\n");

  for(auto s : indices<ShaderStage>())
  {
    statisticsLog.append(QString("%1 calls: %2, non-null shader sets: %3, null shader sets: %4, "
                                 "redundant shader sets: %5\n")
                             .arg(ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                             .arg(shaders[s].calls)
                             .arg(shaders[s].sets)
                             .arg(shaders[s].nulls)
                             .arg(shaders[s].redundants));
  }

  statisticsLog.append(QString("Total calls: %1, non-null shader sets: %2, null shader sets: %3, "
                               "reundant shader sets: %4\n")
                           .arg(totalShadersPerStage.calls)
                           .arg(totalShadersPerStage.sets)
                           .arg(totalShadersPerStage.nulls)
                           .arg(totalShadersPerStage.redundants));
}

void AppendConstantBindStatistics(ICaptureContext &ctx, QString &statisticsLog,
                                  const FrameDescription &frameInfo)
{
  // #mivance C++-side we guarantee all stages will have the same slots
  // and sizes count, so pattern off of the first frame's first stage
  const ConstantBindStats &reference = frameInfo.stats.constants[0];

  // #mivance there is probably a way to iterate the fields via
  // GetType()/GetField() and build a sort of dynamic min/max/average
  // structure for a given type with known integral types (or arrays
  // thereof), but given we're heading for a Qt/C++ rewrite of the UI
  // perhaps best not to dwell too long on that
  ConstantBindStats totalConstantsPerStage[ENUM_ARRAY_SIZE(ShaderStage)];
  memset(&totalConstantsPerStage, 0, sizeof(totalConstantsPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalConstantsPerStage[s].bindslots.create(reference.bindslots.count);
    totalConstantsPerStage[s].sizes.create(reference.sizes.count);
  }

  {
    const ConstantBindStats *constants = frameInfo.stats.constants;
    for(auto s : indices<ShaderStage>())
    {
      totalConstantsPerStage[s].calls += constants[s].calls;
      totalConstantsPerStage[s].sets += constants[s].sets;
      totalConstantsPerStage[s].nulls += constants[s].nulls;

      for(int l = 0; l < constants[s].bindslots.count; l++)
        totalConstantsPerStage[s].bindslots[l] += constants[s].bindslots[l];

      for(int z = 0; z < constants[s].sizes.count; z++)
        totalConstantsPerStage[s].sizes[z] += constants[s].sizes[z];
    }
  }

  ConstantBindStats totalConstantsForAllStages;
  memset(&totalConstantsForAllStages, 0, sizeof(totalConstantsForAllStages));
  totalConstantsForAllStages.bindslots.create(totalConstantsPerStage[0].bindslots.count);
  totalConstantsForAllStages.sizes.create(totalConstantsPerStage[0].sizes.count);

  for(auto s : indices<ShaderStage>())
  {
    const ConstantBindStats &perStage = totalConstantsPerStage[s];
    totalConstantsForAllStages.calls += perStage.calls;
    totalConstantsForAllStages.sets += perStage.sets;
    totalConstantsForAllStages.nulls += perStage.nulls;

    for(int l = 0; l < perStage.bindslots.count; l++)
      totalConstantsForAllStages.bindslots[l] += perStage.bindslots[l];

    for(int z = 0; z < perStage.sizes.count; z++)
      totalConstantsForAllStages.sizes[z] += perStage.sizes[z];
  }

  statisticsLog.append("\n*** Constant Bind Statistics ***\n\n");

  for(auto s : indices<ShaderStage>())
  {
    statisticsLog.append(QString("%1 calls: %2, non-null buffer sets: %3, null buffer sets: %4\n")
                             .arg(ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                             .arg(totalConstantsPerStage[s].calls)
                             .arg(totalConstantsPerStage[s].sets)
                             .arg(totalConstantsPerStage[s].nulls));
  }

  statisticsLog.append(QString("Total calls: %1, non-null buffer sets: %2, null buffer sets: %3\n")
                           .arg(totalConstantsForAllStages.calls)
                           .arg(totalConstantsForAllStages.sets)
                           .arg(totalConstantsForAllStages.nulls));

  statisticsLog.append(CreateSimpleIntegerHistogram(
      "Aggregate slot counts per invocation across all stages", totalConstantsForAllStages.bindslots));

  statisticsLog.append("\nAggregate constant buffer sizes across all stages:\n");
  uint32_t maxCount = 0;
  int maxWithValue = 0;
  for(int s = 0; s < totalConstantsForAllStages.sizes.count; s++)
  {
    uint32_t value = totalConstantsForAllStages.sizes[s];
    if(value > 0)
      maxWithValue = s;
    maxCount = qMax(maxCount, value);
  }

  for(int s = 0; s <= maxWithValue; s++)
  {
    uint32_t count = totalConstantsForAllStages.sizes[s];
    int slice = SliceForString(Stars, count, maxCount);
    statisticsLog.append(QString("%1: %2 %3\n")
                             .arg(Pow2IndexAsReadable(s), 8)
                             .arg(Stars.left(slice))
                             .arg(CountOrEmpty(count)));
  }
}

void AppendSamplerBindStatistics(ICaptureContext &ctx, QString &statisticsLog,
                                 const FrameDescription &frameInfo)
{
  // #mivance see AppendConstantBindStatistics
  const SamplerBindStats &reference = frameInfo.stats.samplers[0];

  SamplerBindStats totalSamplersPerStage[ENUM_ARRAY_SIZE(ShaderStage)];
  memset(&totalSamplersPerStage, 0, sizeof(totalSamplersPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalSamplersPerStage[s].bindslots.create(reference.bindslots.count);
  }

  {
    const SamplerBindStats *samplers = frameInfo.stats.samplers;
    for(auto s : indices<ShaderStage>())
    {
      totalSamplersPerStage[s].calls += samplers[s].calls;
      totalSamplersPerStage[s].sets += samplers[s].sets;
      totalSamplersPerStage[s].nulls += samplers[s].nulls;

      for(int l = 0; l < samplers[s].bindslots.count; l++)
      {
        totalSamplersPerStage[s].bindslots[l] += samplers[s].bindslots[l];
      }
    }
  }

  SamplerBindStats totalSamplersForAllStages;
  memset(&totalSamplersForAllStages, 0, sizeof(totalSamplersForAllStages));
  totalSamplersForAllStages.bindslots.create(totalSamplersPerStage[0].bindslots.count);

  for(auto s : indices<ShaderStage>())
  {
    SamplerBindStats perStage = totalSamplersPerStage[s];
    totalSamplersForAllStages.calls += perStage.calls;
    totalSamplersForAllStages.sets += perStage.sets;
    totalSamplersForAllStages.nulls += perStage.nulls;
    for(int l = 0; l < perStage.bindslots.count; l++)
    {
      totalSamplersForAllStages.bindslots[l] += perStage.bindslots[l];
    }
  }

  statisticsLog.append("\n*** Sampler Bind Statistics ***\n\n");

  for(auto s : indices<ShaderStage>())
  {
    statisticsLog.append(QString("%1 calls: %2, non-null sampler sets: %3, null sampler sets: %4\n")
                             .arg(ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                             .arg(totalSamplersPerStage[s].calls)
                             .arg(totalSamplersPerStage[s].sets)
                             .arg(totalSamplersPerStage[s].nulls));
  }

  statisticsLog.append(
      QString("Total calls: %1, non-null sampler sets: %2, null sampler sets: %3\n")
          .arg(totalSamplersForAllStages.calls)
          .arg(totalSamplersForAllStages.sets)
          .arg(totalSamplersForAllStages.nulls));

  statisticsLog.append(CreateSimpleIntegerHistogram(
      "Aggregate slot counts per invocation across all stages", totalSamplersForAllStages.bindslots));
}

void AppendResourceBindStatistics(ICaptureContext &ctx, QString &statisticsLog,
                                  const FrameDescription &frameInfo)
{
  // #mivance see AppendConstantBindStatistics
  const ResourceBindStats &reference = frameInfo.stats.resources[0];

  ResourceBindStats totalResourcesPerStage[ENUM_ARRAY_SIZE(ShaderStage)];
  memset(&totalResourcesPerStage, 0, sizeof(totalResourcesPerStage));
  for(auto s : indices<ShaderStage>())
  {
    totalResourcesPerStage[s].types.create(reference.types.count);
    totalResourcesPerStage[s].bindslots.create(reference.bindslots.count);
  }

  {
    const ResourceBindStats *resources = frameInfo.stats.resources;
    for(auto s : indices<ShaderStage>())
    {
      totalResourcesPerStage[s].calls += resources[s].calls;
      totalResourcesPerStage[s].sets += resources[s].sets;
      totalResourcesPerStage[s].nulls += resources[s].nulls;

      for(int z = 0; z < resources[s].types.count; z++)
      {
        totalResourcesPerStage[s].types[z] += resources[s].types[z];
      }

      for(int l = 0; l < resources[s].bindslots.count; l++)
      {
        totalResourcesPerStage[s].bindslots[l] += resources[s].bindslots[l];
      }
    }
  }

  ResourceBindStats totalResourcesForAllStages;
  memset(&totalResourcesForAllStages, 0, sizeof(totalResourcesForAllStages));
  totalResourcesForAllStages.types.create(totalResourcesPerStage[0].types.count);
  totalResourcesForAllStages.bindslots.create(totalResourcesPerStage[0].bindslots.count);

  for(auto s : indices<ShaderStage>())
  {
    ResourceBindStats perStage = totalResourcesPerStage[s];
    totalResourcesForAllStages.calls += perStage.calls;
    totalResourcesForAllStages.sets += perStage.sets;
    totalResourcesForAllStages.nulls += perStage.nulls;
    for(int t = 0; t < perStage.types.count; t++)
    {
      totalResourcesForAllStages.types[t] += perStage.types[t];
    }
    for(int l = 0; l < perStage.bindslots.count; l++)
    {
      totalResourcesForAllStages.bindslots[l] += perStage.bindslots[l];
    }
  }

  statisticsLog.append("\n*** Resource Bind Statistics ***\n\n");

  for(auto s : indices<ShaderStage>())
  {
    statisticsLog.append(QString("%1 calls: %2 non-null resource sets: %3 null resource sets: %4\n")
                             .arg(ctx.CurPipelineState().Abbrev(StageFromIndex(s)))
                             .arg(totalResourcesPerStage[s].calls)
                             .arg(totalResourcesPerStage[s].sets)
                             .arg(totalResourcesPerStage[s].nulls));
  }

  statisticsLog.append(
      QString("Total calls: %1 non-null resource sets: %2 null resource sets: %3\n")
          .arg(totalResourcesForAllStages.calls)
          .arg(totalResourcesForAllStages.sets)
          .arg(totalResourcesForAllStages.nulls));

  uint32_t maxCount = 0;
  int maxWithCount = 0;

  statisticsLog.append("\nResource types across all stages:\n");
  for(int s = 0; s < totalResourcesForAllStages.types.count; s++)
  {
    uint32_t count = totalResourcesForAllStages.types[s];
    if(count > 0)
      maxWithCount = s;
    maxCount = qMax(maxCount, count);
  }

  for(int s = 0; s <= maxWithCount; s++)
  {
    uint32_t count = totalResourcesForAllStages.types[s];
    int slice = SliceForString(Stars, count, maxCount);
    TextureDim type = (TextureDim)s;
    statisticsLog.append(
        QString("%1: %2 %3\n").arg(ToQStr(type), 20).arg(Stars.left(slice)).arg(CountOrEmpty(count)));
  }

  statisticsLog.append(CreateSimpleIntegerHistogram(
      "Aggregate slot counts per invocation across all stages", totalResourcesForAllStages.bindslots));
}

void AppendUpdateStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  // #mivance see AppendConstantBindStatistics
  const ResourceUpdateStats &reference = frameInfo.stats.updates;

  ResourceUpdateStats totalUpdates;
  memset(&totalUpdates, 0, sizeof(totalUpdates));
  totalUpdates.types.create(reference.types.count);
  totalUpdates.sizes.create(reference.sizes.count);

  {
    ResourceUpdateStats updates = frameInfo.stats.updates;

    totalUpdates.calls += updates.calls;
    totalUpdates.clients += updates.clients;
    totalUpdates.servers += updates.servers;

    for(int t = 0; t < updates.types.count; t++)
      totalUpdates.types[t] += updates.types[t];

    for(int t = 0; t < updates.sizes.count; t++)
      totalUpdates.sizes[t] += updates.sizes[t];
  }

  statisticsLog.append("\n*** Resource Update Statistics ***\n\n");

  statisticsLog.append(
      QString("Total calls: %1, client-updated memory: %2, server-updated memory: %3\n")
          .arg(totalUpdates.calls)
          .arg(totalUpdates.clients)
          .arg(totalUpdates.servers));

  statisticsLog.append("\nUpdated resource types:\n");
  uint32_t maxCount = 0;
  int maxWithValue = 0;
  for(int s = 1; s < totalUpdates.types.count; s++)
  {
    uint32_t value = totalUpdates.types[s];
    if(value > 0)
      maxWithValue = s;
    maxCount = qMax(maxCount, value);
  }

  for(int s = 1; s <= maxWithValue; s++)
  {
    uint32_t count = totalUpdates.types[s];
    int slice = SliceForString(Stars, count, maxCount);
    TextureDim type = (TextureDim)s;
    statisticsLog.append(
        QString("%1: %2 %3\n").arg(ToQStr(type), 20).arg(Stars.left(slice)).arg(CountOrEmpty(count)));
  }

  statisticsLog.append("\nUpdated resource sizes:\n");
  maxCount = 0;
  maxWithValue = 0;
  for(int s = 0; s < totalUpdates.sizes.count; s++)
  {
    uint32_t value = totalUpdates.sizes[s];
    if(value > 0)
      maxWithValue = s;
    maxCount = qMax(maxCount, value);
  }

  for(int s = 0; s <= maxWithValue; s++)
  {
    uint32_t count = totalUpdates.sizes[s];
    int slice = SliceForString(Stars, count, maxCount);
    statisticsLog.append(QString("%1: %2 %3\n")
                             .arg(Pow2IndexAsReadable(s), 8)
                             .arg(Stars.left(slice))
                             .arg(CountOrEmpty(count)));
  }
}

void AppendBlendStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  BlendStats blends = frameInfo.stats.blends;
  statisticsLog.append("\n*** Blend Statistics ***\n");
  statisticsLog.append(
      QString("Blend calls: %1 non-null sets: %2, null (default) sets: %3, redundant sets: %4\n")
          .arg(blends.calls)
          .arg(blends.sets)
          .arg(blends.nulls)
          .arg(blends.redundants));
}

void AppendDepthStencilStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  DepthStencilStats depths = frameInfo.stats.depths;
  statisticsLog.append("\n*** Depth Stencil Statistics ***\n");
  statisticsLog.append(QString("Depth/stencil calls: %1 non-null sets: %2, null (default) sets: "
                               "%3, redundant sets: %4\n")
                           .arg(depths.calls)
                           .arg(depths.sets)
                           .arg(depths.nulls)
                           .arg(depths.redundants));
}

void AppendRasterizationStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  RasterizationStats rasters = frameInfo.stats.rasters;
  statisticsLog.append("\n*** Rasterization Statistics ***\n");
  statisticsLog.append(QString("Rasterization calls: %1 non-null sets: %2, null (default) sets: "
                               "%3, redundant sets: %4\n")
                           .arg(rasters.calls)
                           .arg(rasters.sets)
                           .arg(rasters.nulls)
                           .arg(rasters.redundants));
  statisticsLog.append(CreateSimpleIntegerHistogram("Viewports set", rasters.viewports));
  statisticsLog.append(CreateSimpleIntegerHistogram("Scissors set", rasters.rects));
}

void AppendOutputStatistics(QString &statisticsLog, const FrameDescription &frameInfo)
{
  OutputTargetStats outputs = frameInfo.stats.outputs;
  statisticsLog.append("\n*** Output Statistics ***\n");
  statisticsLog.append(QString("Output calls: %1 non-null sets: %2, null sets: %3\n")
                           .arg(outputs.calls)
                           .arg(outputs.sets)
                           .arg(outputs.nulls));
  statisticsLog.append(CreateSimpleIntegerHistogram("Outputs set", outputs.bindslots));
}

void AppendDetailedInformation(ICaptureContext &ctx, QString &statisticsLog,
                               const FrameDescription &frameInfo)
{
  if(!frameInfo.stats.recorded)
    return;

  AppendDrawStatistics(statisticsLog, frameInfo);
  AppendDispatchStatistics(statisticsLog, frameInfo);
  AppendInputAssemblerStatistics(statisticsLog, frameInfo);
  AppendShaderStatistics(ctx, statisticsLog, frameInfo);
  AppendConstantBindStatistics(ctx, statisticsLog, frameInfo);
  AppendSamplerBindStatistics(ctx, statisticsLog, frameInfo);
  AppendResourceBindStatistics(ctx, statisticsLog, frameInfo);
  AppendBlendStatistics(statisticsLog, frameInfo);
  AppendDepthStencilStatistics(statisticsLog, frameInfo);
  AppendRasterizationStatistics(statisticsLog, frameInfo);
  AppendUpdateStatistics(statisticsLog, frameInfo);
  AppendOutputStatistics(statisticsLog, frameInfo);
}

void CountContributingEvents(const DrawcallDescription &draw, uint32_t &drawCount,
                             uint32_t &dispatchCount, uint32_t &diagnosticCount)
{
  const DrawFlags diagnosticMask =
      DrawFlags::SetMarker | DrawFlags::PushMarker | DrawFlags::PopMarker;
  DrawFlags diagnosticMasked = draw.flags & diagnosticMask;

  if(diagnosticMasked != DrawFlags::NoFlags)
    diagnosticCount += 1;

  if(draw.flags & DrawFlags::Drawcall)
    drawCount += 1;

  if(draw.flags & DrawFlags::Dispatch)
    dispatchCount += 1;

  for(const DrawcallDescription &c : draw.children)
    CountContributingEvents(c, drawCount, dispatchCount, diagnosticCount);
}

QString AppendAPICallSummary(const FrameDescription &frameInfo, uint numAPICalls)
{
  if(!frameInfo.stats.recorded)
    return "";

  uint numConstantSets = 0;
  uint numSamplerSets = 0;
  uint numResourceSets = 0;
  uint numShaderSets = 0;

  for(auto s : indices<ShaderStage>())
  {
    numConstantSets += frameInfo.stats.constants[s].calls;
    numSamplerSets += frameInfo.stats.samplers[s].calls;
    numResourceSets += frameInfo.stats.resources[s].calls;
    numShaderSets += frameInfo.stats.shaders[s].calls;
  }

  uint numResourceUpdates = frameInfo.stats.updates.calls;
  uint numIndexVertexSets = (frameInfo.stats.indices.calls + frameInfo.stats.vertices.calls +
                             frameInfo.stats.layouts.calls);
  uint numBlendSets = frameInfo.stats.blends.calls;
  uint numDepthStencilSets = frameInfo.stats.depths.calls;
  uint numRasterizationSets = frameInfo.stats.rasters.calls;
  uint numOutputSets = frameInfo.stats.outputs.calls;

  QString calls;
  calls += QString("API calls: %1\n").arg(numAPICalls);
  calls += QString("\tIndex/vertex bind calls: %1\n").arg(numIndexVertexSets);
  calls += QString("\tConstant bind calls: %1\n").arg(numConstantSets);
  calls += QString("\tSampler bind calls: %1\n").arg(numSamplerSets);
  calls += QString("\tResource bind calls: %1\n").arg(numResourceSets);
  calls += QString("\tShader set calls: %1\n").arg(numShaderSets);
  calls += QString("\tBlend set calls: %1\n").arg(numBlendSets);
  calls += QString("\tDepth/stencil set calls: %1\n").arg(numDepthStencilSets);
  calls += QString("\tRasterization set calls: %1\n").arg(numRasterizationSets);
  calls += QString("\tResource update calls: %1\n").arg(numResourceUpdates);
  calls += QString("\tOutput set calls: %1\n").arg(numOutputSets);
  return calls;
}

QString GenerateReport(ICaptureContext &ctx)
{
  QString statisticsLog;

  const rdctype::array<DrawcallDescription> &curDraws = ctx.CurDrawcalls();

  const DrawcallDescription *lastDraw = &curDraws.back();
  while(!lastDraw->children.empty())
    lastDraw = &lastDraw->children.back();

  uint32_t drawCount = 0;
  uint32_t dispatchCount = 0;
  uint32_t diagnosticCount = 0;
  for(const DrawcallDescription &d : curDraws)
    CountContributingEvents(d, drawCount, dispatchCount, diagnosticCount);

  uint32_t numAPIcalls = lastDraw->eventID - (drawCount + dispatchCount + diagnosticCount);

  int numTextures = ctx.GetTextures().count;
  int numBuffers = ctx.GetBuffers().count;

  uint64_t IBBytes = 0;
  uint64_t VBBytes = 0;
  uint64_t BufBytes = 0;
  for(const BufferDescription &b : ctx.GetBuffers())
  {
    BufBytes += b.length;

    if(b.creationFlags & BufferCategory::Index)
      IBBytes += b.length;
    if(b.creationFlags & BufferCategory::Vertex)
      VBBytes += b.length;
  }

  uint64_t RTBytes = 0;
  uint64_t TexBytes = 0;
  uint64_t LargeTexBytes = 0;

  int numRTs = 0;
  float texW = 0, texH = 0;
  float largeTexW = 0, largeTexH = 0;
  int texCount = 0, largeTexCount = 0;
  for(const TextureDescription &t : ctx.GetTextures())
  {
    if(t.creationFlags & (TextureCategory::ColorTarget | TextureCategory::DepthTarget))
    {
      numRTs++;

      RTBytes += t.byteSize;
    }
    else
    {
      texW += (float)t.width;
      texH += (float)t.height;
      texCount++;

      TexBytes += t.byteSize;

      if(t.width > 32 && t.height > 32)
      {
        largeTexW += (float)t.width;
        largeTexH += (float)t.height;
        largeTexCount++;

        LargeTexBytes += t.byteSize;
      }
    }
  }

  texW /= texCount;
  texH /= texCount;

  largeTexW /= largeTexCount;
  largeTexH /= largeTexCount;

  const FrameDescription &frameInfo = ctx.FrameInfo();

  float compressedMB = (float)frameInfo.compressedFileSize / (1024.0f * 1024.0f);
  float uncompressedMB = (float)frameInfo.uncompressedFileSize / (1024.0f * 1024.0f);
  float compressRatio = uncompressedMB / compressedMB;
  float persistentMB = (float)frameInfo.persistentSize / (1024.0f * 1024.0f);
  float initDataMB = (float)frameInfo.initDataSize / (1024.0f * 1024.0f);

  QString header =
      QString(
          "Stats for %1.\n\nFile size: %2MB (%3MB uncompressed, compression ratio %4:1)\n"
          "Persistent Data (approx): %5MB, Frame-initial data (approx): %6MB\n")
          .arg(QFileInfo(ctx.LogFilename()).fileName())
          .arg(compressedMB, 2, 'f', 2)
          .arg(uncompressedMB, 2, 'f', 2)
          .arg(compressRatio, 2, 'f', 2)
          .arg(persistentMB, 2, 'f', 2)
          .arg(initDataMB, 2, 'f', 2);
  QString drawList =
      QString("Draw calls: %1\nDispatch calls: %2\n").arg(drawCount).arg(dispatchCount);
  QString calls = AppendAPICallSummary(frameInfo, numAPIcalls);
  QString ratio = QString("API:Draw/Dispatch call ratio: %1\n\n")
                      .arg((float)numAPIcalls / (float)(drawCount + dispatchCount));
  QString textures = QString(
                         "%1 Textures - %2 MB (%3 MB over 32x32), %4 RTs - %5 MB.\n"
                         "Avg. tex dimension: %6x%7 (%8x%9 over 32x32)\n")
                         .arg(numTextures)
                         .arg((float)TexBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                         .arg((float)LargeTexBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                         .arg(numRTs)
                         .arg((float)RTBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                         .arg(texW)
                         .arg(texH)
                         .arg(largeTexW)
                         .arg(largeTexH);
  QString buffers = QString("%1 Buffers - %2 MB total %3 MB IBs %4 MB VBs.\n")
                        .arg(numBuffers)
                        .arg((float)BufBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                        .arg((float)IBBytes / (1024.0f * 1024.0f), 2, 'f', 2)
                        .arg((float)VBBytes / (1024.0f * 1024.0f), 2, 'f', 2);
  QString load = QString("%1 MB - Grand total GPU buffer + texture load.\n")
                     .arg((float)(TexBytes + BufBytes + RTBytes) / (1024.0f * 1024.0f), 2, 'f', 2);

  statisticsLog.append(header);

  statisticsLog.append("\n*** Summary ***\n\n");
  statisticsLog.append(drawList);
  statisticsLog.append(calls);
  statisticsLog.append(ratio);
  statisticsLog.append(textures);
  statisticsLog.append(buffers);
  statisticsLog.append(load);

  AppendDetailedInformation(ctx, statisticsLog, frameInfo);

  return statisticsLog;
}

StatisticsViewer::StatisticsViewer(ICaptureContext &ctx, QWidget *parent)
    : QFrame(parent), ui(new Ui::StatisticsViewer), m_Ctx(ctx)
{
  ui->setupUi(this);

  ui->statistics->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

  m_Ctx.AddLogViewer(this);
}

StatisticsViewer::~StatisticsViewer()
{
  m_Ctx.BuiltinWindowClosed(this);

  m_Ctx.RemoveLogViewer(this);
  delete ui;
}

void StatisticsViewer::OnLogfileClosed()
{
  ui->statistics->clear();
}

void StatisticsViewer::OnLogfileLoaded()
{
  ui->statistics->setText(GenerateReport(m_Ctx));
}