#include "tp_pipeline_image_utils/step_delegates/ExtractRectStepDelegate.h"
#include "tp_data_image_utils/members/ByteMapMember.h"
#include "tp_data_image_utils/members/ColorMapMember.h"
#include "tp_data_image_utils/members/LineCollectionMember.h"
#include "tp_data_image_utils/members/GridMember.h"

#include "tp_image_utils_functions/ExtractRect.h"

#include "tp_image_utils/Grid.h"

#include "tp_pipeline/StepDetails.h"
#include "tp_pipeline/StepInput.h"

#include "tp_data/Collection.h"

#include "tp_utils/StackTrace.h"
#include "tp_utils/DebugUtils.h"

#include <math.h>

namespace tp_pipeline_image_utils
{

namespace
{
//##################################################################################################
enum class AreaMode_lt
{
  Rect,
  Area,
  Grid
};

//##################################################################################################
AreaMode_lt areaModeFromString(const std::string& mode)
{
  if(mode=="Area") return AreaMode_lt::Area;
  if(mode=="Grid") return AreaMode_lt::Grid;
  return AreaMode_lt::Rect;
}

//##################################################################################################
void _fixupParameters(tp_pipeline::StepDetails* stepDetails)
{
  stepDetails->setOutputNames({"Output data"});

  std::vector<tp_utils::StringID> validParams;
  const auto& parameters = stepDetails->parameters();

  {
    tp_utils::StringID name = modeSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The type of area to cut out.";
    param.setEnum({"Rect",
                   "Area",
                   "Grid"});
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  AreaMode_lt areaMode = areaModeFromString(stepDetails->parameterValue<std::string>(modeSID()));

  {
    tp_utils::StringID name = originModeSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "How to calculate the x,y coords of the rect.";
    param.setEnum(ExtractRectStepDelegate::originModeStrings());
    param.enabled = areaMode==AreaMode_lt::Rect;
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  auto originMode = ExtractRectStepDelegate::originModeFromString(stepDetails->parameterValue<std::string>(originModeSID()));

  {
    tp_utils::StringID name = colorImageSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The source image to cut the shape from.";
    param.type = tp_pipeline::namedDataSID();
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    tp_utils::StringID name = destinationWidthSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The width of the image generated by this step.";
    param.type = tp_pipeline::sizeSID();
    param.min = size_t(0);
    param.max = size_t(10000);
    param.validateBounds<size_t>(0);
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    tp_utils::StringID name = destinationHeightSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The height of the image generated by this step.";
    param.type = tp_pipeline::sizeSID();
    param.min = size_t(0);
    param.max = size_t(10000);
    param.validateBounds<size_t>(0);
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    tp_utils::StringID name = xSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The x origin of this rect.";
    param.type = tp_pipeline::sizeSID();
    param.min = size_t(0);
    param.max = size_t(10000);
    param.validateBounds<size_t>(0);
    param.enabled = areaMode==AreaMode_lt::Rect && originMode==ExtractRectStepDelegate::OriginMode::XY;
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    tp_utils::StringID name = ySID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The y origin of this rect.";
    param.type = tp_pipeline::sizeSID();
    param.min = size_t(0);
    param.max = size_t(10000);
    param.validateBounds<size_t>(0);
    param.enabled = areaMode==AreaMode_lt::Rect && originMode==ExtractRectStepDelegate::OriginMode::XY;
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    tp_utils::StringID name = clippingAreaSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The shape to cut out.";
    param.type = tp_pipeline::namedDataSID();
    param.enabled = areaMode==AreaMode_lt::Area;
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }


  {
    tp_utils::StringID name = clippingGridSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The grid to cut out.";
    param.type = "Named grids";
    param.enabled = areaMode==AreaMode_lt::Grid;
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  stepDetails->setParametersOrder(validParams);
  stepDetails->setValidParameters(validParams);
}

}

//##################################################################################################
ExtractRectStepDelegate::ExtractRectStepDelegate():
  AbstractStepDelegate(extractRectSID(), {findAndSegmentSID()})
{

}

//##################################################################################################
void ExtractRectStepDelegate::executeStep(tp_pipeline::StepDetails* stepDetails,
                                          const tp_pipeline::StepInput& input,
                                          tp_data::Collection& output) const
{  
  size_t width  = stepDetails->parameterValue<size_t>( destinationWidthSID());
  size_t height = stepDetails->parameterValue<size_t>(destinationHeightSID());
  size_t x      = stepDetails->parameterValue<size_t>(                xSID());
  size_t y      = stepDetails->parameterValue<size_t>(                ySID());

  AreaMode_lt areaMode = areaModeFromString(stepDetails->parameterValue<std::string>(modeSID()));  
  auto originMode = originModeFromString(stepDetails->parameterValue<std::string>(originModeSID()));

  std::string clippingAreaName = stepDetails->parameterValue<std::string>(clippingAreaSID());
  std::string clippingGridName = stepDetails->parameterValue<std::string>(clippingGridSID());

  //-- This is the image to cut the shape from -----------------------------------------------------
  const tp_data_image_utils::ColorMapMember* src{nullptr};
  {
    std::string sourceImageName = stepDetails->parameterValue<std::string>(colorImageSID());
    input.memberCast(sourceImageName, src);

    if(!src)
    {
      output.addError("Failed to find source image.");
      return;
    }
  }

  //-- The area to cut out -------------------------------------------------------------------------
  const tp_data_image_utils::LineCollectionMember* clippingArea{nullptr};
  input.memberCast(clippingAreaName, clippingArea);

  //-- Or a grid to cut out ------------------------------------------------------------------------
  const tp_data_image_utils::GridMember* clippingGrid = nullptr;
  input.memberCast(clippingGridName, clippingGrid);

  //-- Make sure that we have either a clipping area or a grid -------------------------------------
  if(areaMode == AreaMode_lt::Area && (!clippingArea || clippingArea->data.empty()))
  {
    output.addError("Failed to find clipping area!");
    return;
  }

  //-- Make sure that we have either a clipping area or a grid -------------------------------------
  if(areaMode == AreaMode_lt::Grid && !clippingGrid)
  {
    output.addError("Failed to find clipping grid!");
    return;
  }

  std::vector<std::string> errors;

  if(src->data.size()>0)
  {
    if(width<1)
    {
      if(clippingGrid && clippingGrid->data.xCells>0)
        width = size_t(float(clippingGrid->data.xCells) * ceil(clippingGrid->data.xAxis.length()));
      else
        width = size_t(src->data.width());
    }

    if(height<1)
    {
      if(clippingGrid && clippingGrid->data.yCells>0)
        height = size_t(float(clippingGrid->data.yCells) * ceil(clippingGrid->data.yAxis.length()));
      else
        height = size_t(src->data.height());
    }

    if(areaMode == AreaMode_lt::Area)
    {
      if(clippingArea && !clippingArea->data.empty())
      {
        auto outMember = new tp_data_image_utils::ColorMapMember(stepDetails->lookupOutputName("Output data"));
        output.addMember(outMember);
        outMember->data = tp_image_utils_functions::ExtractRect::extractRect(src->data,
                                                                             clippingArea->data.at(0),
                                                                             width,
                                                                             height,
                                                                             errors);
      }
    }

    else if(areaMode == AreaMode_lt::Grid)
    {
      if(clippingGrid)
      {
        auto outMember = new tp_data_image_utils::ColorMapMember(stepDetails->lookupOutputName("Output data"));
        output.addMember(outMember);
        outMember->data = tp_image_utils_functions::ExtractRect::extractRect(src->data,
                                                                             clippingGrid->data,
                                                                             width,
                                                                             height, errors);
      }
    }

    else if(areaMode == AreaMode_lt::Rect)
    {
      if(width>0 && height>0)
      {
        if(originMode==OriginMode::CenterCrop)
        {
          if(src->data.width()>width)
            x = (src->data.width()-width) / 2;
          if(src->data.height()>height)
            x = (src->data.height()-height) / 2;
        }

        auto outMember = new tp_data_image_utils::ColorMapMember(stepDetails->lookupOutputName("Output data"));
        output.addMember(outMember);
        outMember->data = tp_image_utils_functions::ExtractRect::extractRect(src->data,
                                                                             x,
                                                                             y,
                                                                             width,
                                                                             height,
                                                                             TPPixel(0, 0, 0));
      }
    }

    for(const auto& error : errors)
      output.addError(error);
  }
}

//##################################################################################################
void ExtractRectStepDelegate::fixupParameters(tp_pipeline::StepDetails* stepDetails) const
{
  _fixupParameters(stepDetails);
}

//##################################################################################################
const std::vector<std::string>& ExtractRectStepDelegate::originModeStrings()
{
  static std::vector<std::string> sizeCalculationStrings{"XY",
                                                         "CenterCrop"};
  return sizeCalculationStrings;
}

//##################################################################################################
ExtractRectStepDelegate::OriginMode ExtractRectStepDelegate::originModeFromString(const std::string& originMode)
{
  if(originMode=="XY")
    return ExtractRectStepDelegate::OriginMode::XY;
  if(originMode=="CenterCrop")
    return ExtractRectStepDelegate::OriginMode::CenterCrop;
  return ExtractRectStepDelegate::OriginMode::XY;
}

//##################################################################################################
std::string ExtractRectStepDelegate::originModeToString(ExtractRectStepDelegate::OriginMode originMode)
{
  switch (originMode)
  {
  case ExtractRectStepDelegate::OriginMode::XY:         return "XY";
  case ExtractRectStepDelegate::OriginMode::CenterCrop: return "CenterCrop";
  }

  return "XY";
}

//##################################################################################################
tp_pipeline::StepDetails* ExtractRectStepDelegate::makeStepDetails(const std::string& inName,
                                                                   const std::string& outName,
                                                                   ExtractRectStepDelegate::OriginMode originMode,
                                                                   size_t width,
                                                                   size_t height)
{
  auto stepDetails = new tp_pipeline::StepDetails(scaleSID());
  _fixupParameters(stepDetails);
  stepDetails->setParameterValue(colorImageSID(), inName);
  stepDetails->setOutputMapping({{"Output data", outName}});
  stepDetails->setParameterValue(originModeSID(), originModeToString(originMode));
  stepDetails->setParameterValue( destinationWidthSID(), width );
  stepDetails->setParameterValue(destinationHeightSID(), height);
  return stepDetails;
}

}
