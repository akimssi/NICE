// The MIT License (MIT)
//
// Copyright (c) 2016 Northeastern University
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#ifndef CPP_INCLUDE_KMEANS_H
#define CPP_INCLUDE_KMEANS_H

#include "include/matrix.h"
#include "include/vector.h"
#include <vector>
#include <map>
#include <algorithm>
#include "dlib/clustering.h"


namespace Nice {

enum InitMethodEnum{RANDOM, KMEANSPP, MANUAL};

template<typename T>
class KMeans {
 public:
  void Fit1(const Matrix<T> &input_data, int k) {
    int num_features = input_data.cols();
    int num_samples = input_data.rows();
    typedef dlib::matrix<T> sample_type;
    typedef dlib::radial_basis_kernel<sample_type> kernel_type;
    std::vector<sample_type> samples;
    std::vector<sample_type> initial_centers;
    sample_type m;
    m.set_size(num_features, 1);
    for (long i = 0; i < num_samples; i++) {
      for (long j = 0; j < num_features; j++)
        m(j) = input_data(i, j);
      samples.push_back(m);
    }
    dlib::kcentroid<kernel_type> kc(kernel_type(0.01), 0.0001, 20);
    dlib::kkmeans<kernel_type> km(kc);
    km.set_number_of_centers(k);
    dlib::pick_initial_centers(k, initial_centers, samples, km.get_kernel());
    km.train(samples, initial_centers);
//    Vector<T> assignments(num_samples);
    Labels = Vector<T>::Zero(num_samples);
    for (long i = 0; i < num_samples; i++) {
//      std::cout << samples[i] << std::endl;
      Labels(i) = km(samples[i]);
    }
  }

  void Fit(const Matrix<T> &input_data, int k) {
    K = k;
    centers_.resize(input_data.rows(), K);
    Cluster(input_data);
  }

  void Cluster(const Matrix<T> &input_data)
  {
    if(input_data.cols() < K)
    {
      std::stringstream ss;
      ss << "The number of points (" << input_data.cols()
         << ") must be larger than the number of clusters (" << K << ")";
      throw std::runtime_error(ss.str());
    }
    
    // Seed a random number generator
    if(Random)
    {
      unsigned int t = time(NULL);
      srand48(t);
    }
    else
    {
      srand48(0);
    }
  
    if(InitMethod == RANDOM)
    {
      RandomInit(input_data);
    }
    else if(InitMethod == KMEANSPP) // http://en.wikipedia.org/wiki/K-means%2B%2B
    {
      KMeansPPInit(input_data);
    }
    else if(InitMethod == MANUAL)
    {
      // do nothing, the cluster centers should have been provided manually
    }
    else
    {
      throw std::runtime_error("An invalid initialization method has been specified!");
    }
  
    // We must store the labels at the previous iteration to determine whether any labels changed at each iteration.
    Vector<T> oldLabels = Vector<T>::Zero(input_data.cols()); // initialize to all zeros
  
    // Initialize the labels array
    Labels.resize(input_data.cols());
  
    // The current iteration number
    int iter = 0;
  
    // Track whether any labels changed in the last iteration
    bool changed = true;
    do
    {
      AssignLabels(input_data);
  
      EstimateNewCenters(input_data);
      
      changed = CheckChanged(Labels, oldLabels);
  
      // Save the old labels
      oldLabels = Labels;
      iter++;
    }while(changed);
    //}while(iter < 100); // You could use this stopping criteria to make kmeans run for a specified number of iterations
  
    std::cout << "KMeans finished in " << iter << " iterations." << std::endl;
  }
  
  std::vector<unsigned int> GetIndicesWithLabel(const T label) const
  {
    std::vector<unsigned int> pointsWithLabel;
    for(unsigned int i = 0; i < Labels.size(); i++)
    {
      if(Labels(i) == label)
      {
        pointsWithLabel.push_back(i);
      }
    }
  
    return pointsWithLabel;
  }
  
  Matrix<T> GetPointsWithLabel(const Matrix<T> &input_data, const T label) const
  {
    std::vector<unsigned int> indicesWithLabel = GetIndicesWithLabel(label);
  
    Matrix<T> pointsWithLabel(input_data.rows(), indicesWithLabel.size());
  
    for(unsigned int i = 0; i < indicesWithLabel.size(); i++)
    {
      pointsWithLabel.col(i) = input_data.col(indicesWithLabel[i]);
    }
  
    return pointsWithLabel;
  }
  
  unsigned int SelectWeightedIndex(Vector<T> weights)
  {
  
    // Normalize
    Vector<T> normalizedWeights = weights / weights.sum();

    std::map<T, unsigned int> weights_index_map;
    for (unsigned int i = 0; i < weights.size(); i++) {
      weights_index_map[normalizedWeights(i)] = i;
    }
  
    // Sort the normalized weights
    std::sort(normalizedWeights.data(), normalizedWeights.data()+normalizedWeights.size());

    // Get the 
    T randomValue = (T)drand48();
  
    T runningTotal = 0.0;
    for(unsigned int i = 0; i < normalizedWeights.size(); i++)
    {
      runningTotal += normalizedWeights[i];
      if(randomValue < runningTotal)
      {
        T weight = normalizedWeights(i);
        return weights_index_map[weight];
      }
    }
  
    std::cerr << "runningTotal: " << runningTotal << std::endl;
    std::cerr << "randomValue: " << randomValue << std::endl;
    throw std::runtime_error("SelectWeightedIndex() reached end, we should never get here.");
  
    return 0;
  }
  
  
  bool CheckChanged(const Vector<T>& labels, const Vector<T>& oldLabels)
  {
    bool changed = false;
    for(unsigned int i = 0; i < labels.size(); i++)
    {
      if(labels(i) != oldLabels(i)) //if something changed
      {
        changed = true;
        break;
      }
    }
    return changed;
  }
  
  void AssignLabels(const Matrix<T> &input_data)
  {
    // Assign each point to the closest cluster
    for(unsigned int point = 0; point < input_data.cols(); ++point)
    {
      unsigned int closestCluster = FindClosestCluster(input_data.col(point), K);
      Labels(point) = (T)closestCluster;
    }
  }
  
  void EstimateNewCenters(const Matrix<T> &input_data)
  {
    Matrix<T> oldCenters = centers_;
  
    for(unsigned int cluster = 0; cluster < K; ++cluster)
    {
      std::vector<unsigned int> indicesWithLabel = GetIndicesWithLabel(cluster);
      Matrix<T> classPoints(input_data.rows(), indicesWithLabel.size());
      for(unsigned int point = 0; point < indicesWithLabel.size(); point++)
      {
        classPoints.col(point) = input_data.col(indicesWithLabel[point]);
      }
  
      Vector<T> center;
      if(classPoints.cols() == 0)
      {
        center = oldCenters.col(cluster);
      }
      else
      {
        center = classPoints.rowwise().mean();
      }
  
      centers_.col(cluster) = center;
    }
  }
  
  unsigned int FindClosestCluster(const Vector<T>& queryPoint, unsigned int num_cluster)
  {
    unsigned int closestCluster = 0;
    T minDist = std::numeric_limits<T>::max();
    for(unsigned int i = 0; i < num_cluster; ++i)
    {
      T dist = (centers_.col(i) - queryPoint).norm();
      if(dist < minDist)
      {
        minDist = dist;
        closestCluster = i;
      }
    }
  
    return closestCluster;
  }
  
  unsigned int ClosestPointIndex(const Matrix<T> &input_data, const Vector<T> queryPoint)
  {
    unsigned int closestPoint = 0;
    T minDist = std::numeric_limits<T>::max();
    for(unsigned int i = 0; i < input_data.cols(); i++)
    {
      //double dist = sqrt(vtkMath::Distance2BetweenPoints(points->GetPoint(i), queryPoint));
      T dist = (input_data.col(i) - queryPoint).norm();
      if(dist < minDist)
      {
        minDist = dist;
        closestPoint = i;
      }
    }
  
    return closestPoint;
  }
  
  T ClosestPointDistanceExcludingId(const Matrix<T> &input_data, Vector<T> queryPoint, const unsigned int excludedId)
  {
    std::vector<unsigned int> excludedIds;
    excludedIds.push_back(excludedId);
    return ClosestPointDistanceExcludingIds(input_data, queryPoint, excludedIds);
  }
  
  T ClosestPointDistanceExcludingIds(const Matrix<T> &input_data,
                                     const Vector<T> &queryPoint,
                                     const std::vector<unsigned int> excludedIds)
  {
    T minDist = std::numeric_limits<T>::infinity();
    for(unsigned int pointId = 0; pointId < input_data.cols(); ++pointId)
    {
      bool contains = false;
      for(unsigned int i = 0; i < excludedIds.size(); ++i) {
        if (excludedIds[i] == pointId) {
          contains = true;
          break;
        }
      }
      if(contains)
      {
        continue;
      }
      T dist = (input_data.col(pointId) - queryPoint).norm();
  
      if(dist < minDist)
      {
        minDist = dist;
      }
    }
    return minDist;
  }
  
  T ClosestPointDistance(const Vector<T>& queryPoint)
  {
    std::vector<unsigned int> excludedIds; // none
    return ClosestPointDistanceExcludingIds(queryPoint, excludedIds);
  }

  Vector<T> GetRandomPointInBounds(const Matrix<T> &input_data)
  {
    Vector<T> minVector = input_data.rowwise().minCoeff();
    Vector<T> maxVector = input_data.rowwise().maxCoeff();
  
    Vector<T> randomVector = Vector<T>::Zero(minVector.size());
  
    for(int i = 0; i < randomVector.size(); ++i)
    {
      T range = maxVector(i) - minVector(i);
      T randomValue = (T)drand48() * range + minVector(i);
      randomVector(i) = randomValue;
    }
  
    return randomVector;
  }

  void RandomInit(const Matrix<T> &input_data)
  {
    // Completely randomly choose initial cluster centers
    for(unsigned int i = 0; i < K; i++)
    {
      centers_.col(i) = GetRandomPointInBounds(input_data);
    }
  }
  
  void KMeansPPInit(const Matrix<T> &input_data)
  {
    // Assign one center at random
    unsigned int randomId = rand() % input_data.cols();
    Vector<T> p = input_data.col(randomId);
    centers_.col(0) = p;
  
    // Assign the rest of the initial centers using a weighted probability of the distance to the nearest center
    Vector<T> weights(input_data.cols());
    for(unsigned int cluster = 1; cluster < K; ++cluster) // Start at 1 because cluster 0 is already set
    {
      // Create weight vector
      for(unsigned int i = 0; i < input_data.cols(); i++)
      {
        Vector<T> currentPoint = input_data.col(i);
        unsigned int closestCluster = FindClosestCluster(currentPoint, cluster);
        weights(i) = (centers_.col(closestCluster) - currentPoint).squaredNorm();
      }
  
      unsigned int selectedPointId = SelectWeightedIndex(weights);
      p = input_data.col(selectedPointId);
      centers_.col(cluster) = p;
    }
  }
  
  void SetRandom(const bool r)
  {
    this->Random = r;
  }
  
  void SetInitMethod(const int method)
  {
    this->InitMethod = method;
  }
  
  
  T ComputeMLEVariance(const Matrix<T> &input_data) const
  {
    // \hat{\sigma}^2 = \frac{1}{n} \sum_{i=1}^n (x_i - \hat{x}_i)^2
  
    T overall_variance = 0;
    for(unsigned int i = 0; i < K; ++i)
    {
      std::vector<unsigned int> indicesWithLabel = GetIndicesWithLabel(i);
      T variance = 0;
      if(indicesWithLabel.size() == 0)
      {
          variance = 0;
      }
  
      // \hat{\sigma}^2 = \frac{1}{n} \sum_{i=1}^n (x_i - \hat{x}_i)^2
  
      Vector<T> clusterCenter = centers_.col(i);
  
      for(unsigned int i = 0; i < indicesWithLabel.size(); ++i)
      {
        T error = (clusterCenter - input_data.col(indicesWithLabel[i])).norm();
        variance += error;
      }
  
      variance /= static_cast<T>(indicesWithLabel.size());     
      overall_variance += variance;
    }
  
    return overall_variance;
  }

  Vector<T> GetLabels() {
    return Labels;
  }

 private:
  Vector<T> Labels;

  /** Should the computation be random? If false, then it is repeatable (for testing). */
  bool Random = true;

  /** The initialization method to use. */
  int InitMethod = KMEANSPP;

  /** The number of clusters to find. */
  unsigned int K;
  
  /** The current cluster centers. */
  Matrix<T> centers_;
 
};
}
#endif  // CPP_INCLUDE_KMEANS_H
