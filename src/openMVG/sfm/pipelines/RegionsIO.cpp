#include "RegionsIO.hpp"
#include "third_party/progress/progress.hpp"

#include <atomic>


namespace openMVG {
namespace sfm {


std::unique_ptr<features::Regions> loadRegions(const std::string& folder, IndexT id_view, const features::Image_describer& imageDescriber)
{
  const std::string imageDescriberTypeName = features::EImageDescriberType_enumToString(imageDescriber.getDescriberType());
  const std::string basename = std::to_string(id_view);

  const std::string featFilename = stlplus::create_filespec(folder, basename, imageDescriberTypeName + ".feat");
  const std::string descFilename = stlplus::create_filespec(folder, basename, imageDescriberTypeName + ".desc");
  OPENMVG_LOG_WARNING("featFilename: " << featFilename);
  OPENMVG_LOG_WARNING("descFilename: " << descFilename);

  std::unique_ptr<features::Regions> regionsPtr;
  imageDescriber.Allocate(regionsPtr);

  if (!regionsPtr->Load(featFilename, descFilename))
  {
    OPENMVG_LOG_WARNING("Invalid " << imageDescriberTypeName << " regions files for the view : " << basename << ", describer type: " << imageDescriberTypeName);
    OPENMVG_LOG_WARNING("See features and descriptors files: " << featFilename << "\n" << descFilename);
    return std::unique_ptr<features::Regions>();
  }
  OPENMVG_LOG_WARNING("RegionCount: " << regionsPtr->RegionCount());
  return regionsPtr;
}

bool loadRegionsPerView(features::RegionsPerView& regionsPerView,
            const SfM_Data& sfmData,
            const std::string& folder,
            const std::vector<features::EImageDescriberType>& imageDescriberTypes,
            const std::set<IndexT>& viewIdFilter)
{
  C_Progress_display my_progress_bar( sfmData.GetViews().size() * imageDescriberTypes.size(), std::cout, "\n- Regions Loading -\n");

  std::atomic_bool invalid(false);


  std::vector< std::unique_ptr<features::Image_describer> > imageDescribers;
  imageDescribers.resize(imageDescriberTypes.size());

  for(std::size_t i =0; i < imageDescriberTypes.size(); ++i)
  {
    imageDescribers[i] = createImageDescriber(imageDescriberTypes[i]);
  }

#ifdef OPENMVG_USE_OPENMP
  #pragma omp parallel num_threads(3)
#endif
 for (auto iter = sfmData.GetViews().begin();
   iter != sfmData.GetViews().end() && !invalid; ++iter)
 {
#ifdef OPENMVG_USE_OPENMP
  #pragma omp single nowait
#endif
   {
     for(std::size_t i = 0; i < imageDescriberTypes.size(); ++i)
     {
       if(viewIdFilter.empty() || viewIdFilter.find(iter->second.get()->id_view) != viewIdFilter.end())
       {
         std::unique_ptr<features::Regions> regionsPtr = loadRegions(folder, iter->second.get()->id_view, *imageDescribers[i]);

         if(regionsPtr)
         {
#ifdef OPENMVG_USE_OPENMP
  #pragma omp critical
#endif
           {
             regionsPerView.addRegions(iter->second.get()->id_view, imageDescriberTypes[i], regionsPtr.release());
             ++my_progress_bar;
           }
         }
         else
         {
           invalid = true;
         }
       }
     }
   }
 }
 return !invalid;
}


bool loadFeaturesPerView(features::FeaturesPerView& featuresPerView,
                      const SfM_Data& sfmData,
                      const std::string& storageDirectory,
                      const std::vector<features::EImageDescriberType>& imageDescriberTypes)
{
  C_Progress_display my_progress_bar( sfmData.GetViews().size(), std::cout, "\n- Features Loading -\n" );

  // Read for each view the corresponding features and store them as PointFeatures
  std::atomic_bool invalid(false);

  std::vector< std::unique_ptr<features::Image_describer> > imageDescribers;
  imageDescribers.resize(imageDescriberTypes.size());

  for(std::size_t i =0; i < imageDescriberTypes.size(); ++i)
  {
    imageDescribers[i] = createImageDescriber(imageDescriberTypes[i]);
  }

#ifdef OPENMVG_USE_OPENMP
  #pragma omp parallel
#endif
  for (auto iter = sfmData.GetViews().begin();
    (iter != sfmData.GetViews().end()) && (!invalid); ++iter)
  {
#ifdef OPENMVG_USE_OPENMP
  #pragma omp single nowait
#endif
    {
      for(std::size_t i = 0; i < imageDescriberTypes.size(); ++i)
      {
        const std::string featFile = stlplus::create_filespec(storageDirectory,
                std::to_string(iter->second->id_view),
                EImageDescriberType_enumToString(imageDescriberTypes[i]) + ".feat");

        OPENMVG_LOG_WARNING("featFilename: " << featFile);

        std::unique_ptr<features::Regions> regionsPtr;
        imageDescribers[i]->Allocate(regionsPtr);

        if (!regionsPtr->LoadFeatures(featFile))
        {
          OPENMVG_LOG_WARNING("Invalid feature file: " << featFile);
          invalid = true;
        }

#ifdef OPENMVG_USE_OPENMP
  #pragma omp critical
#endif
        {
          // save loaded Features as PointFeature
          featuresPerView.addFeatures(iter->second.get()->id_view, imageDescriberTypes[i], regionsPtr->GetRegionsPositions());
          ++my_progress_bar;
        }
      }
    }
  }
  return !invalid;
}

  
} // namespace sfm
} // namespace openMVG
