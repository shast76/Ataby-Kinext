#include "pose.hpp"
#include <opencv2/imgproc.hpp>
#include <algorithm>
#include <iostream>
#include <stdexcept>

#ifdef USE_DIRECTML
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <dml_provider_factory.h>
#endif

PoseEstimator::PoseEstimator(const std::filesystem::path& modelPath, bool useGpu) {
    Ort::SessionOptions opts;
    opts.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);

#ifdef USE_DIRECTML
#ifndef NOMINMAX
#define NOMINMAX
#endif
    if (useGpu) {
        try {
            // DirectML: Windows'ta ve Xbox'ta GPU ile calisir
            opts.DisableMemPattern();
            opts.SetExecutionMode(ExecutionMode::ORT_SEQUENTIAL);
            Ort::ThrowOnError(OrtSessionOptionsAppendExecutionProvider_DML(opts, 0));
            backend_ = "DirectML (GPU)";
        } catch (const std::exception& e) {
            std::cerr << "DirectML acilamadi, CPU kullaniliyor: " << e.what() << "\n";
        }
    }
#else
    (void)useGpu;
#endif

    session_ = Ort::Session(env_, modelPath.native().c_str(), opts);

    Ort::AllocatorWithDefaultOptions alloc;
    inputName_ = session_.GetInputNameAllocated(0, alloc).get();
    outputName_ = session_.GetOutputNameAllocated(0, alloc).get();

    auto info = session_.GetInputTypeInfo(0).GetTensorTypeAndShapeInfo();
    inputType_ = info.GetElementType();
    auto shape = info.GetShape();   // [1,192,192,3] (NHWC) ya da [1,3,192,192] (NCHW)
    if (shape.size() != 4) throw std::runtime_error("Beklenmeyen model girisi (4 boyut olmali)");
    nchw_ = (shape[1] == 3);
    int64_t s = nchw_ ? shape[2] : shape[1];
    inputSize_ = s > 0 ? static_cast<int>(s) : 192;   // dinamik boyutsa 192 (Lightning)

    std::cout << "Model: giris '" << inputName_ << "' " << inputSize_ << "x" << inputSize_
              << (nchw_ ? " NCHW" : " NHWC") << ", cikis '" << outputName_ << "', " << backend_ << "\n";
}

bool PoseEstimator::estimate(const cv::Mat& bgr, Keypoints& out) {
    if (bgr.empty()) return false;

    // Bakilacak bolge: kisi biliniyorsa onun etrafi, degilse tum goruntu (kare)
    const float fullSide = static_cast<float>((std::max)(bgr.cols, bgr.rows));
    cv::Rect2f region = hasCrop_ ? crop_ : cv::Rect2f(0, 0, fullSide, fullSide);
    const int rs = (std::max)(1, cvRound(region.width));

    // Bolgeyi siyah bir kareye kopyala (goruntu disina tasan kisimlar siyah kalir)
    cv::Mat patch(rs, rs, bgr.type(), cv::Scalar(0, 0, 0));
    cv::Rect src(cvRound(region.x), cvRound(region.y), rs, rs);
    cv::Rect inImg = src & cv::Rect(0, 0, bgr.cols, bgr.rows);
    if (inImg.area() <= 0) { hasCrop_ = false; return false; }
    bgr(inImg).copyTo(patch(cv::Rect(inImg.x - src.x, inImg.y - src.y, inImg.width, inImg.height)));

    cv::Mat rgb, resized;
    cv::cvtColor(patch, rgb, cv::COLOR_BGR2RGB);
    cv::resize(rgb, resized, cv::Size(inputSize_, inputSize_), 0, 0, cv::INTER_AREA);

    const size_t n = static_cast<size_t>(inputSize_) * inputSize_ * 3;
    std::array<int64_t, 4> shape = nchw_
        ? std::array<int64_t, 4>{1, 3, inputSize_, inputSize_}
        : std::array<int64_t, 4>{1, inputSize_, inputSize_, 3};
    auto mem = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);

    auto pixel = [&](size_t i) -> uint8_t {
        if (!nchw_) return resized.data[i];
        size_t plane = static_cast<size_t>(inputSize_) * inputSize_;
        size_t c = i / plane, p = i % plane;
        return resized.data[p * 3 + c];
    };

    Ort::Value input{ nullptr };
    switch (inputType_) {
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32:
        bufI_.resize(n);
        for (size_t i = 0; i < n; ++i) bufI_[i] = pixel(i);
        input = Ort::Value::CreateTensor<int32_t>(mem, bufI_.data(), n, shape.data(), 4);
        break;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_UINT8:
        bufU8_.resize(n);
        for (size_t i = 0; i < n; ++i) bufU8_[i] = pixel(i);
        input = Ort::Value::CreateTensor<uint8_t>(mem, bufU8_.data(), n, shape.data(), 4);
        break;
    case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT:
        bufF_.resize(n);
        for (size_t i = 0; i < n; ++i) bufF_[i] = static_cast<float>(pixel(i));
        input = Ort::Value::CreateTensor<float>(mem, bufF_.data(), n, shape.data(), 4);
        break;
    default:
        throw std::runtime_error("Desteklenmeyen model giris tipi");
    }

    const char* inNames[] = { inputName_.c_str() };
    const char* outNames[] = { outputName_.c_str() };
    auto outputs = session_.Run(Ort::RunOptions{ nullptr }, inNames, &input, 1, outNames, 1);

    // Cikis: [1,1,17,3] -> (y, x, skor) bolgeye gore 0-1; goruntu pikseline cevir
    const float* d = outputs[0].GetTensorData<float>();
    size_t count = outputs[0].GetTensorTypeAndShapeInfo().GetElementCount();
    if (count < COCO_COUNT * 3) return false;
    for (int i = 0; i < COCO_COUNT; ++i) {
        out[i].y = region.y + d[i * 3 + 0] * region.height;
        out[i].x = region.x + d[i * 3 + 1] * region.width;
        out[i].score = d[i * 3 + 2];
    }

    updateCrop(out, bgr.cols, bgr.rows);
    return true;
}

// Bir sonraki kare icin kisinin etrafinda kare bir bolge belirler (MoveNet'in onerdigi yontem)
void PoseEstimator::updateCrop(const Keypoints& kp, int imgW, int imgH) {
    const float minScore = 0.2f;
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    int good = 0;
    for (const auto& k : kp) {
        if (k.score < minScore) continue;
        minX = (std::min)(minX, k.x); maxX = (std::max)(maxX, k.x);
        minY = (std::min)(minY, k.y); maxY = (std::max)(maxY, k.y);
        ++good;
    }
    bool torsoOk = kp[L_SHOULDER].score > minScore && kp[R_SHOULDER].score > minScore &&
                   kp[L_HIP].score > minScore && kp[R_HIP].score > minScore;
    if (good < 6 || !torsoOk) { hasCrop_ = false; return; }   // kisi kayboldu: tum goruntuye don

    // Govde boyu: bacaklar gorunmese bile tum vucudu kapsayacak buyuklugu tahmin etmek icin
    float sx = (kp[L_SHOULDER].x + kp[R_SHOULDER].x) / 2, sy = (kp[L_SHOULDER].y + kp[R_SHOULDER].y) / 2;
    float hx = (kp[L_HIP].x + kp[R_HIP].x) / 2,           hy = (kp[L_HIP].y + kp[R_HIP].y) / 2;
    float torso = std::hypot(sx - hx, sy - hy);

    float cx = (minX + maxX) / 2;
    float cy = hy;                                              // merkez: kalca (ust + alt vucut dengeli)
    float half = (std::max)({ (maxX - minX) * 0.5f, (maxY - minY) * 0.5f, torso * 2.3f }) * 1.25f;
    float full = static_cast<float>((std::max)(imgW, imgH));
    half = (std::min)(half, full * 0.5f);

    crop_ = cv::Rect2f(cx - half, cy - half, half * 2, half * 2);
    hasCrop_ = true;
}
