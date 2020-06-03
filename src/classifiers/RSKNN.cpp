// Developed by: Rakib

#include <iostream>
#include <vector>
#include <fstream>
#include <string>
#include <map>

#include <yaml-cpp/yaml.h>
#include <ros/package.h>

#include <opencv2/opencv.hpp>
#if CV_MAJOR_VERSION == 2
#include <opencv2/highgui/highgui.hpp>
#include <opencv2/imgproc/imgproc.hpp>
#include <opencv2/core/core.hpp>
#include <opencv2/ml/ml.hpp>
#elif CV_MAJOR_VERSION == 3
#include <opencv2/core.hpp>
#include <opencv2/highgui.hpp>
#include <opencv2/imgproc.hpp>
#include <opencv2/ml.hpp>
#endif

#include <robosherlock/scene_cas.h>
#include <robosherlock/types/all_types.h>
#include <robosherlock/scene_cas.h>
#include <robosherlock/utils/time.h>
#include <robosherlock/DrawingAnnotator.h>

#include <rs_addons/classifiers/RSKNN.h>
#include <uima/api.hpp>

using namespace cv;

//..............................k-Nearest Neighbor Classifier.........................................
RSKNN::RSKNN(int K)
{
  knncalld = cv::ml::KNearest::create();
  knncalld->setDefaultK(K);
}

void RSKNN:: trainModel(std::string train_matrix_name, std::string train_label_name, std::string train_label_n)
{
}

void RSKNN:: classify(std::string trained_file_name_saved, std::string test_matrix_name, std::string test_label_name, std::string obj_classInDouble)
{
}

void RSKNN::classifyOnLiveData(std::string trained_file_name_saved, cv::Mat test_mat, double &det, double &confi)
{
}

void RSKNN::loadModelFile(std::string pathToModelFile)
{
   readFeaturesFromFile(pathToModelFile, "", trainingData_, dataLabels_);
   knncalld->train(trainingData_, cv::ml::ROW_SAMPLE, dataLabels_);
}

void RSKNN:: classifyKNN(std::string train_matrix_name, std::string train_label_name,
                         std::string test_matrix_name, std::string test_label_name, std::string obj_classInDouble, int default_k)
{
  //To load the train data................
  cv::Mat train_matrix;
  cv::Mat train_label;
  readFeaturesFromFile(train_matrix_name, train_label_name, train_matrix, train_label);
  std::cout << "size of train matrix:" << train_matrix.size() << std::endl;
  std::cout << "size of train label:" << train_label.size() << std::endl;


  //To load the test data.............................
  cv::Mat test_matrix;
  cv::Mat test_label;
  readFeaturesFromFile(test_matrix_name, test_label_name, test_matrix, test_label);
  std::cout << "size of test matrix :" << test_matrix.size() << std::endl;
  std::cout << "size of test label" << test_label.size() << std::endl;

#if CV_MAJOR_VERSION == 2
  CvKNearest *knncalld = new CvKNearest;

  //Train the classifier...................................
  knncalld->train(train_matrix, train_label, cv::Mat(), false, default_k, false);

  //To get the value of k.............
  int k_max = knncalld->get_max_k();
  //cv::Mat neighborResponses, bestResponse, distances;

#elif CV_MAJOR_VERSION == 3
  int k_max = knncalld->getDefaultK();
#endif

  //convert test label matrix into a vector.......................
  std::vector<double> con_test_label;
  test_label.col(0).copyTo(con_test_label);

  //Container to hold the integer value of labels............................
  std::vector<int> actual_label;
  std::vector<int> predicted_label;

  for(int i = 0; i < test_label.rows; i++) {
#if CV_MAJOR_VERSION == 2
    // double res = knncls->find_nearest(test_matrix.row(i), k,bestResponse,neighborResponses,distances);
    double res = knncalld->find_nearest(test_matrix.row(i), k_max);
#elif CV_MAJOR_VERSION == 3

    double res = knncalld->findNearest(test_matrix.row(i), k_max, cv::noArray());
#endif

    int prediction = res;
    predicted_label.push_back(prediction);
    double lab = con_test_label[i];
    int actual_convert = lab;
    actual_label.push_back(actual_convert);
  }
  std::cout << "K-Nearest Neighbor Result :" << std::endl;
  evaluation(actual_label, predicted_label, obj_classInDouble);
}

std::pair<double,double> RSKNN::classifyOnLiveDataKNN(cv::Mat test_mat)
{
  int k_max = knncalld->getDefaultK();


  cv::normalize(test_mat, test_mat, 1, 0, cv::NORM_L2);

  cv::Mat results, neighborResponses, dists;
  double res = knncalld->findNearest(test_mat, k_max, results, neighborResponses, dists);
  outInfo(dists);
  double confidence = (2 - dists.at<float>(0))/2;

  return std::pair<double, double>(res,confidence);
}

void  RSKNN::processPCLFeatureKNN(std::string set_mode, std::string feature_use,
                                  std::vector<rs::ObjectHypothesis> clusters, cv::Mat &color, std::vector<std::string> models_label, uima::CAS &tcas)
{
  outInfo("Number of cluster:" << clusters.size());

  for(size_t i = 0; i < clusters.size(); ++i) {
    rs::ObjectHypothesis &cluster = clusters[i];
    std::vector<rs::PclFeature> features;
    cluster.annotations.filter(features);

    for(size_t j = 0; j < features.size(); ++j) {
      rs::PclFeature &feats = features[j];
      outInfo("type of feature:" << feats.feat_type() << std::endl);
      std::vector<float> featDescriptor = feats.feature();
      outInfo("Size after conversion:" << featDescriptor.size());
      cv::Mat test_mat(1, featDescriptor.size(), CV_32F);

      for(size_t k = 0; k < featDescriptor.size(); ++k) {
        test_mat.at<float>(0, k) = featDescriptor[k];
      }
      outInfo("number of elements in :" << i);
      std::pair<double, double> result = classifyOnLiveDataKNN(test_mat);
      int classLabelInInt = result.first;
      double confidence = result.second;

      std::string classLabelInString = models_label[classLabelInInt - 1];
      outInfo("prediction class is :" << classLabelInInt <<"class label being: " <<classLabelInString<<" with confidence: "<<confidence);

      //To annotate the clusters..................
      annotate_hypotheses(tcas, classLabelInString, feature_use, cluster, set_mode, confidence);

      //set roi on image
      rs::ImageROI image_roi = cluster.rois.get();
      cv::Rect rect;
      rs::conversion::from(image_roi.roi_hires.get(), rect);

      //Draw result on image...........
      drawCluster(color, rect, classLabelInString);

      outInfo("calculation is done" << std::endl);
    }
  }
}

void  RSKNN::processCaffeFeatureKNN(std::string set_mode, std::string feature_use, std::vector<rs::ObjectHypothesis> clusters,
                                    cv::Mat &color, std::vector<std::string> models_label, uima::CAS &tcas)
{
  //clusters comming from RS pipeline............................
  outInfo("Number of cluster:" << clusters.size() << std::endl);

  for(size_t i = 0; i < clusters.size(); ++i) {
    rs::ObjectHypothesis &cluster = clusters[i];
    std::vector<rs::Features> features;
    cluster.annotations.filter(features);

    for(size_t j = 0; j < features.size(); ++j) {
      rs::Features &feats = features[j];
      //variable to store caffe feature..........
      cv::Mat featDescriptor;

      if(feats.source() == "Caffe") {
        rs::conversion::from(feats.descriptors(), featDescriptor);
        outInfo("Size after conversion:" << featDescriptor.size());

        //The function generate the prediction result................

        std::pair<double, double> result = classifyOnLiveDataKNN(featDescriptor);
        int classLabelInInt = result.first;
        double confidence = result.second;

        //class label in integer, which is used as index of vector model_label.
        std::string classLabelInString = models_label[classLabelInInt - 1];
        outInfo("prediction class is :" << classLabelInInt <<" class label being: " <<classLabelInString<<" with confidence: "<<confidence);
        //To annotate the clusters..................
        annotate_hypotheses(tcas, classLabelInString, feature_use, cluster, set_mode, confidence);

        //set roi on image
        rs::ImageROI image_roi = cluster.rois.get();
        cv::Rect rect;
        rs::conversion::from(image_roi.roi_hires.get(), rect);

        //Draw result on image...........................
        drawCluster(color, rect, classLabelInString, confidence);
      }
      outInfo("calculation is done");
    }
  }
}

void RSKNN::annotate_hypotheses(uima::CAS &tcas, std::string class_name, std::string feature_name, rs::ObjectHypothesis &cluster, std::string set_mode, double &confi)
{ if(confi> 0.6)
  {rs::Classification classResult = rs::create<rs::Classification>(tcas);
  classResult.classname.set(class_name);
  classResult.classifier("k-Nearest Neighbor");
  classResult.featurename(feature_name);
  classResult.source.set("Knn");
  if(feature_name == "BVLC_REF") {
    classResult.classification_type("INSTANCE");
  }
  else if(feature_name == "VFH") {
    classResult.classification_type("INSTANCE");
  }

  if(set_mode == "CL") {
    cluster.annotations.append(classResult);
  }
  else if(set_mode == "GT") {
    rs::GroundTruth setGT = rs::create<rs::GroundTruth>(tcas);
    setGT.classificationGT.set(classResult);
    cluster.annotations.append(setGT);
  }
  else {
    outError("You should set the parameter (set_mode) as CL or GT");
  }}
}

RSKNN::~RSKNN()
{
}
