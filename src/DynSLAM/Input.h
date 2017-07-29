
#ifndef DYNSLAM_INPUT_H
#define DYNSLAM_INPUT_H

#include <string>
#include <highgui.h>
#include <memory>

#include "DepthProvider.h"
#include "Utils.h"
#include "../InfiniTAM/InfiniTAM/ITMLib/Objects/ITMRGBDCalib.h"

namespace dynslam {

/// \brief Provides input from DynSLAM, in the form of RGBD frames.
/// Since DynSLAM currently operates with stereo input, this class also computes depth from stereo.
/// Currently, this is "computed" by reading the depth maps from disk, but the plan is to compute
/// depth on the fly in the future.
class Input {
 public:
  struct Config {
    std::string dataset_name;
    std::string left_gray_folder;
    std::string right_gray_folder;
    std::string left_color_folder;
    std::string right_color_folder;
    std::string fname_format;
    std::string itm_calibration_fname;

    // These are optional, and only used for precomputed depth/segmentation.
    std::string depth_folder = "";
    std::string depth_fname_format = "";
    // Whether we read direct metric depth from the file, or just disparity values expressed in
    // pixels.
    bool read_depth = false;
    // No format specifier for segmentation information, since the segmented frames' names are based
    // on the RGB frame file names. See `PrecomputedSegmentationProvider` for more information.
    std::string segmentation_folder = "";

    // Whether to read ground truth odometry information from an OxTS dump folder (e.g., KITTI
    // dataset), or from a single-file ground truth, as provided with the kitti-odometry dataset.
    bool odometry_oxts = false;   // TODO(andrei): Support this.
    std::string odometry_fname = "";

    /// \brief The velodyne LIDAR data used only for evaluation.
    std::string velodyne_folder = "";
    std::string velodyne_fname_format = "";
  };

  /// We don't use constants here in order to make the code easier to read.
  static Config KittiOdometryConfig() {
    Config config;
    config.dataset_name           = "kitti-odometry";
    config.left_gray_folder       = "image_0";
    config.right_gray_folder      = "image_1";
    config.left_color_folder      = "image_2";
    config.right_color_folder     = "image_3";
    config.fname_format           = "%06d.png";
    config.itm_calibration_fname  = "itm-calib.txt";

    config.depth_folder           = "precomputed-depth/Frames";
    config.depth_fname_format     = "%04d.pgm";
    config.read_depth             = true;

    config.segmentation_folder    = "seg_image_2/mnc";

    config.odometry_oxts          = false;
    config.odometry_fname         = "ground-truth-poses.txt";

    config.velodyne_folder        = "velodyne";
    config.velodyne_fname_format  = "%06d.bin";

    return config;
  };

  static Config KittiOdometryDispnetConfig() {
    Config config = KittiOdometryConfig();
    config.depth_folder           = "precomputed-depth-dispnet";
    config.depth_fname_format     = "%06d.pfm";
    config.read_depth             = false;
    return config;
  }

 public:
  Input(const std::string &dataset_folder,
        const Config &config,
        DepthProvider *depth_provider,
        const ITMLib::Objects::ITMRGBDCalib &calibration,
        const StereoCalibration &stereo_calibration,
        int frame_offset = 0)
      : dataset_folder_(dataset_folder),
        config_(config),
        depth_provider_(depth_provider),
        frame_idx_(frame_offset),
        calibration_(calibration),
        stereo_calibration_(stereo_calibration),
        depth_buf_(static_cast<int>(calibration.intrinsics_d.sizeY),
                   static_cast<int>(calibration.intrinsics_d.sizeX))
  {}

  bool HasMoreImages();

  /// \brief Advances the input reader to the next frame.
  /// \returns True if the next frame's files could be read successfully.
  bool ReadNextFrame();

  /// \brief Returns pointers to the latest RGB and depth data.
  /// \note The caller does not take ownership.
  void GetCvImages(cv::Mat3b **rgb, cv::Mat1s **raw_depth);

  /// \brief Returns pointers to the latest grayscale input frames.
  void GetCvStereoGray(cv::Mat1b **left, cv::Mat1b **right);

  cv::Size2i GetRgbSize() const {
    return cv::Size2i(static_cast<int>(calibration_.intrinsics_rgb.sizeX),
                      static_cast<int>(calibration_.intrinsics_rgb.sizeY));
  }

  cv::Size2i GetDepthSize() const {
    return cv::Size2i(static_cast<int>(calibration_.intrinsics_d.sizeX),
                      static_cast<int>(calibration_.intrinsics_d.sizeY));
  }

  /// \brief Gets the name of the dataset folder which we are using.
  /// TODO(andrei): Make this more robust.
  std::string GetSequenceName() const {
    return dataset_folder_.substr(dataset_folder_.rfind('/') + 1);
  }

  std::string GetDatasetIdentifier() const {
    return config_.dataset_name + "-" + GetSequenceName();
  }

  DepthProvider* GetDepthProvider() const {
    return depth_provider_;
  }

  void SetDepthProvider(DepthProvider *depth_provider) {
    this->depth_provider_ = depth_provider;
  }

  /// \brief Returns the current frame index from the dataset.
  /// \note May not be the same as the current DynSLAM frame number if an offset was used.
  int GetCurrentFrame() const {
    return frame_idx_;
  }

  /// \brief Sets the out parameters to the RGB and depth images from the specified frame.
  void GetFrameCvImages(int frame_idx, std::shared_ptr<cv::Mat3b> &rgb, std::shared_ptr<cv::Mat1s> &raw_depth);

  const Config& GetConfig() const {
    return config_;
  }

 private:
  std::string dataset_folder_;
  Config config_;
  DepthProvider *depth_provider_;
  int frame_idx_;
  // TODO-LOW(andrei): get rid of this and replace with a similar object which doesn't require ITM.
  ITMLib::Objects::ITMRGBDCalib calibration_;
  StereoCalibration stereo_calibration_;

  cv::Mat3b left_frame_color_buf_;
  cv::Mat3b right_frame_color_buf_;
  cv::Mat1s depth_buf_;

  // Store the grayscale information necessary for scene flow computation using libviso2, and
  // on-the-fly depth map computation using libelas.
  cv::Mat1b left_frame_gray_buf_;
  cv::Mat1b right_frame_gray_buf_;

  static std::string GetFrameName(const std::string &root,
                                  const std::string &folder,
                                  const std::string &fname_format,
                                  int frame_idx) {
    return root + "/" + folder + "/" + utils::Format(fname_format, frame_idx);
  }

  void ReadLeftGray(int frame_idx, cv::Mat1b &out) const;
  void ReadRightGray(int frame_idx, cv::Mat1b &out) const;
  void ReadLeftColor(int frame_idx, cv::Mat3b &out) const;
  void ReadRightColor(int frame_idx, cv::Mat3b &out) const;
};

} // namespace dynslam

#endif //DYNSLAM_INPUT_H
