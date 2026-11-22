// Kinext 1.0 - ONNX Runtime ile MoveNet poz tahmini
#pragma once
#include "skeleton.hpp"
#include <onnxruntime_cxx_api.h>
#include <opencv2/core.hpp>
#include <filesystem>
#include <string>
#include <vector>

class PoseEstimator {
public:
    PoseEstimator(const std::filesystem::path& modelPath, bool useGpu);
    // BGR kamera karesinden 17 nokta (piksel koordinati) cikarir
    bool estimate(const cv::Mat& bgr, Keypoints& out);
    const std::string& backend() const { return backend_; }

private:
    Ort::Env env_{ ORT_LOGGING_LEVEL_WARNING, "Kinext" };
    Ort::Session session_{ nullptr };
    std::string inputName_, outputName_, backend_ = "CPU";
    int inputSize_ = 192;
    ONNXTensorElementDataType inputType_ = ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
    bool nchw_ = false;
    // Kisiye odakli kirpma: onceki karede kisinin bulundugu kare bolge (goruntu pikseli)
    cv::Rect2f crop_;
    bool hasCrop_ = false;
    void updateCrop(const Keypoints& kp, int imgW, int imgH);

    std::vector<int32_t> bufI_;
    std::vector<float> bufF_;
    std::vector<uint8_t> bufU8_;
};
