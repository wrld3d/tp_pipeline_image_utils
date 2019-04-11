#include "tp_pipeline_image_utils/step_delegates/ScaleStepDelegate.h"
#include "tp_data_image_utils/members/ColorMapMember.h"

#include "tp_image_utils/Scale.h"

#include "tp_pipeline/StepDetails.h"
#include "tp_pipeline/StepInput.h"

#include "tp_data/Collection.h"

namespace tp_pipeline_image_utils
{
namespace
{
//##################################################################################################
std::pair<size_t,size_t> calculateSize(size_t width, size_t height, size_t srcWidth, size_t srcHeight)
{
  std::pair<size_t, size_t> size(width, height);

  if(width==0 && height>0)
  {
    float f = float(height) / float(srcHeight);
    size.first = size_t(float(srcWidth) * f);
  }
  else if(height==0 && width>0)
  {
    float f = float(width) / float(srcWidth);
    size.second = size_t(float(srcHeight) * f);
  }

  if(size.first<1)
    size.first = srcWidth;

  if(size.second<1)
    size.second = srcHeight;

  return size;
}

//##################################################################################################
void _fixupParameters(tp_pipeline::StepDetails* stepDetails)
{
  stepDetails->setOutputNames({"Output color image", "Output byte map"});

  std::vector<tp_utils::StringID> validParams;
  const auto& parameters = stepDetails->parameters();

  {
    const tp_utils::StringID& name = destinationWidthSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The width of the image generated by this step.";
    param.type = tp_pipeline::sizeSID();
    param.min = size_t(1);
    param.max = size_t(10000);

    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    const tp_utils::StringID& name = destinationHeightSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The height of the image generated by this step.";
    param.type = tp_pipeline::sizeSID();
    param.min = size_t(1);
    param.max = size_t(10000);

    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    const tp_utils::StringID& name = colorImageSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The source image to scale.";
    param.type = tp_pipeline::namedDataSID();

    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    const tp_utils::StringID& name = byteMapSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The source byte map to scale.";
    param.type = tp_pipeline::namedDataSID();
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  {
    const tp_utils::StringID& name = functionSID();
    auto param = tpGetMapValue(parameters, name);
    param.name = name;
    param.description = "The function to use for downscaling images.";
    param.setEnum({"Default", "Custom"});
    stepDetails->setParamerter(param);
    validParams.push_back(name);
  }

  stepDetails->setParametersOrder(validParams);
  stepDetails->setValidParameters(validParams);
}
}

//##################################################################################################
ScaleStepDelegate::ScaleStepDelegate():
  AbstractStepDelegate(scaleSID(), {processingSID()})
{

}

//##################################################################################################
void ScaleStepDelegate::executeStep(tp_pipeline::StepDetails* stepDetails,
                                    const tp_pipeline::StepInput& input,
                                    tp_data::Collection& output) const
{
  size_t width         = stepDetails->parameterValue<size_t>     ( destinationWidthSID());
  size_t height        = stepDetails->parameterValue<size_t>     (destinationHeightSID());
  //std::string function = stepDetails->parameterValue<std::string>(         functionSID());

  std::string colorImageName = stepDetails->parameterValue<std::string>(colorImageSID());
  std::string    byteMapName = stepDetails->parameterValue<std::string>(byteMapSID());

  if(!colorImageName.empty())
  {
    const tp_data_image_utils::ColorMapMember* src{nullptr};
    input.memberCast(colorImageName, src);
    if(src)
    {
      auto outMember = new tp_data_image_utils::ColorMapMember(stepDetails->lookupOutputName("Output color image"));
      output.addMember(outMember);
      std::pair<size_t, size_t> size = calculateSize(width, height, src->data.width(), src->data.height());
      outMember->data = tp_image_utils::scale(src->data, size.first, size.second);
    }
    else
    {
      output.addError("Failed to find color image.");
    }
  }

  if(!byteMapName.empty())
  {
    const tp_data_image_utils::ColorMapMember* src{nullptr};
    input.memberCast(byteMapName, src);
    if(src)
    {
      auto outMember = new tp_data_image_utils::ColorMapMember(stepDetails->lookupOutputName("Output byte map"));
      output.addMember(outMember);
      std::pair<size_t, size_t> size = calculateSize(width, height, src->data.width(), src->data.height());
      outMember->data = tp_image_utils::scale(src->data, size.first, size.second);
    }
    else
    {
      output.addError("Failed to find byte map image.");
    }
  }
}

//##################################################################################################
void ScaleStepDelegate::fixupParameters(tp_pipeline::StepDetails* stepDetails) const
{
  _fixupParameters(stepDetails);
}

//##################################################################################################
tp_pipeline::StepDetails* ScaleStepDelegate::makeStepDetails(int width, int height)
{
  auto stepDetails = new tp_pipeline::StepDetails(scaleSID());
  _fixupParameters(stepDetails);
  stepDetails->setParameterValue(destinationWidthSID(),  width );
  stepDetails->setParameterValue(destinationHeightSID(), height);
  return stepDetails;
}

}
