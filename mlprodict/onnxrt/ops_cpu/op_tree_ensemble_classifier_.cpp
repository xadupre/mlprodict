// Inspired from 
// https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_classifier.cc.

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <vector>
#include <thread>
#include <iterator>

#ifndef SKIP_PYTHON
//#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
//#include <numpy/arrayobject.h>

namespace py = pybind11;
#endif


enum class POST_EVAL_TRANSFORM {
  NONE,
  LOGISTIC,
  SOFTMAX,
  SOFTMAX_ZERO,
  PROBIT
};

POST_EVAL_TRANSFORM to_POST_EVAL_TRANSFORM(const std::string &value)
{
    if (value.compare("NONE") == 0) return POST_EVAL_TRANSFORM::NONE;
    if (value.compare("LOGISTIC") == 0) return POST_EVAL_TRANSFORM::LOGISTIC;
    if (value.compare("SOFTMAX") == 0) return POST_EVAL_TRANSFORM::SOFTMAX;
    if (value.compare("SOFTMAX_ZERO") == 0) return POST_EVAL_TRANSFORM::SOFTMAX_ZERO;
    if (value.compare("PROBIT") == 0) return POST_EVAL_TRANSFORM::PROBIT;
    throw std::runtime_error(std::string("NODE_MODE '") + 
                             value + 
                             std::string(" is not defined."));
}

enum class NODE_MODE {
  BRANCH_LEQ,
  BRANCH_LT,
  BRANCH_GTE,
  BRANCH_GT,
  BRANCH_EQ,
  BRANCH_NEQ,
  LEAF
};

NODE_MODE to_NODE_MODE(const std::string &value)
{
    if (value.compare("BRANCH_LEQ") == 0) return NODE_MODE::BRANCH_LEQ;
    if (value.compare("BRANCH_LT") == 0) return NODE_MODE::BRANCH_LT;
    if (value.compare("BRANCH_GTE") == 0) return NODE_MODE::BRANCH_GTE;
    if (value.compare("BRANCH_GT") == 0) return NODE_MODE::BRANCH_GT;
    if (value.compare("BRANCH_EQ") == 0) return NODE_MODE::BRANCH_EQ;
    if (value.compare("BRANCH_NEQ") == 0) return NODE_MODE::BRANCH_NEQ;
    if (value.compare("LEAF") == 0) return NODE_MODE::LEAF;
    throw std::runtime_error(std::string("NODE_MODE '") + 
                             value + 
                             std::string(" is not defined."));
}


class RuntimeTreeEnsembleClassifier
{
    public:

        // tree_ensemble_classifier.h
        std::vector<int64_t> nodes_treeids_;
        std::vector<int64_t> nodes_nodeids_;
        std::vector<int64_t> nodes_featureids_;
        std::vector<float> nodes_values_;
        std::vector<float> nodes_hitrates_;
        //std::vector<std::string> nodes_modes_names_;
        std::vector<NODE_MODE> nodes_modes_;
        std::vector<int64_t> nodes_truenodeids_;
        std::vector<int64_t> nodes_falsenodeids_;
        std::vector<int64_t> missing_tracks_true_;  // no bool type

        std::vector<int64_t> class_nodeids_;
        std::vector<int64_t> class_treeids_;
        std::vector<int64_t> class_ids_;
        std::vector<float> class_weights_;
        int64_t class_count_;
        std::set<int64_t> weights_classes_;

        std::vector<float> base_values_;
        //std::vector<std::string> classlabels_strings_;
        std::vector<int64_t> classlabels_int64s_;

        std::vector<std::tuple<int64_t, int64_t, int64_t, float>> leafnodedata_;
        std::unordered_map<int64_t, int64_t> leafdata_map_;
        std::vector<int64_t> roots_;
        const int64_t kOffset_ = 4000000000L;
        const int64_t kMaxTreeDepth_ = 1000;
        POST_EVAL_TRANSFORM post_transform_;
        bool weights_are_all_positive_;
    
    public:
        
        RuntimeTreeEnsembleClassifier();
        ~RuntimeTreeEnsembleClassifier();

        void init(
            py::array_t<float> base_values, // 0
            py::array_t<int64_t> class_ids, // 1
            py::array_t<int64_t> class_nodeids, // 2
            py::array_t<int64_t> class_treeids, // 3
            py::array_t<float> class_weights, // 4
            py::array_t<int64_t> classlabels_int64s, // 5
            const std::vector<std::string>& classlabels_strings, // 6
            py::array_t<int64_t> nodes_falsenodeids, // 7
            py::array_t<int64_t> nodes_featureids, // 8
            py::array_t<float> nodes_hitrates, // 9
            py::array_t<int64_t> nodes_missing_value_tracks_true, // 10
            const std::vector<std::string>& nodes_modes, // 11
            py::array_t<int64_t> nodes_nodeids, // 12
            py::array_t<int64_t> nodes_treeids, // 13
            py::array_t<int64_t> nodes_truenodeids, // 14
            py::array_t<float> nodes_values, // 15
            const std::string& post_transform // 16
        );
        
        py::tuple compute(py::array_t<float> X) const;

        void ProcessTreeNode(std::map<int64_t, float>& classes,
                             int64_t treeindex,
                             const float* x_data,
                             int64_t feature_base) const;

    private:

        void Initialize();
};


#define array2vector(vec, arr, dtype) { \
    if (arr.size() > 0) { \
        auto n = arr.size(); \
        auto p = (dtype*) arr.data(0); \
        vec = std::vector<dtype>(p, p + n); \
    } \
}

#define arrayshape2vector(vec, arr) { \
    if (arr.size() > 0) { \
        vec.resize(arr.ndim()); \
        for(int i = 0; i < vec.size(); ++i) \
            vec[i] = (int64_t) arr.shape(i); \
    } \
}


static const float ml_sqrt2 = 1.41421356f;


RuntimeTreeEnsembleClassifier::RuntimeTreeEnsembleClassifier() {
}


RuntimeTreeEnsembleClassifier::~RuntimeTreeEnsembleClassifier() {
}



void RuntimeTreeEnsembleClassifier::init(
            py::array_t<float> base_values,
            py::array_t<int64_t> class_ids,
            py::array_t<int64_t> class_nodeids,
            py::array_t<int64_t> class_treeids,
            py::array_t<float> class_weights,
            py::array_t<int64_t> classlabels_int64s,
            const std::vector<std::string>& classlabels_strings,
            py::array_t<int64_t> nodes_falsenodeids,
            py::array_t<int64_t> nodes_featureids,
            py::array_t<float> nodes_hitrates,
            py::array_t<int64_t> nodes_missing_value_tracks_true,
            const std::vector<std::string>& nodes_modes,
            py::array_t<int64_t> nodes_nodeids,
            py::array_t<int64_t> nodes_treeids,
            py::array_t<int64_t> nodes_truenodeids,
            py::array_t<float> nodes_values,
            const std::string& post_transform
    ) {
    array2vector(nodes_treeids_, nodes_treeids, int64_t);
    array2vector(nodes_nodeids_, nodes_nodeids, int64_t);
    array2vector(nodes_featureids_, nodes_featureids, int64_t);
    array2vector(nodes_values_, nodes_values, float);
    array2vector(nodes_hitrates_, nodes_hitrates, float);
    array2vector(nodes_truenodeids_, nodes_truenodeids, int64_t);
    array2vector(nodes_falsenodeids_, nodes_falsenodeids, int64_t);
    array2vector(missing_tracks_true_, nodes_missing_value_tracks_true, int64_t);
    //nodes_modes_names_ = nodes_modes;
    array2vector(class_nodeids_, class_nodeids, int64_t);
    array2vector(class_treeids_, class_treeids, int64_t);
    array2vector(class_ids_, class_ids, int64_t);
    array2vector(class_weights_, class_weights, float);
    array2vector(base_values_, base_values, float);
    if (classlabels_strings.size() > 0)
        throw std::runtime_error("This runtime only handles integers.");
    // classlabels_strings_ = classlabels_strings;
    array2vector(classlabels_int64s_, classlabels_int64s, int64_t);
    post_transform_ = to_POST_EVAL_TRANSFORM(post_transform);

    // additional members
    nodes_modes_.resize(nodes_modes.size());
    for(int i = 0; i < nodes_modes.size(); ++i)
        nodes_modes_[i] = to_NODE_MODE(nodes_modes[i]);

    Initialize();
}

void RuntimeTreeEnsembleClassifier::Initialize() {
  int64_t current_tree_id = 1234567891L;
  std::vector<int64_t> tree_offsets;
  weights_are_all_positive_ = true;

  for (int64_t i = 0, size_node_treeids = static_cast<int64_t>(nodes_treeids_.size());
       i < size_node_treeids;
       ++i) {
    if (nodes_treeids_[i] != current_tree_id) {
      tree_offsets.push_back(nodes_nodeids_[i]);
      current_tree_id = nodes_treeids_[i];
    }
    int64_t offset = tree_offsets[tree_offsets.size() - 1];
    nodes_nodeids_[i] = nodes_nodeids_[i] - offset;
    if (nodes_falsenodeids_[i] >= 0) {
      nodes_falsenodeids_[i] = nodes_falsenodeids_[i] - offset;
    }
    if (nodes_truenodeids_[i] >= 0) {
      nodes_truenodeids_[i] = nodes_truenodeids_[i] - offset;
    }
  }
  for (int64_t i = 0, size_class_nodeids = static_cast<int64_t>(class_nodeids_.size());
       i < size_class_nodeids;
       ++i) {
    int64_t offset = tree_offsets[class_treeids_[i]];
    class_nodeids_[i] = class_nodeids_[i] - offset;
    if (class_weights_[i] < 0) {
      weights_are_all_positive_ = false;
    }
  }

  // leafnode data, these are the votes that leaves do
  for (size_t i = 0, end = class_nodeids_.size(); i < end; ++i) {
    leafnodedata_.push_back(std::make_tuple(class_treeids_[i], class_nodeids_[i], class_ids_[i], class_weights_[i]));
    weights_classes_.insert(class_ids_[i]);
  }
  std::sort(std::begin(leafnodedata_), std::end(leafnodedata_), [](auto const& t1, auto const& t2) {
    if (std::get<0>(t1) != std::get<0>(t2))
      return std::get<0>(t1) < std::get<0>(t2);

    return std::get<1>(t1) < std::get<1>(t2);
  });
  // make an index so we can find the leafnode data quickly when evaluating
  int64_t field0 = -1;
  int64_t field1 = -1;
  for (size_t i = 0, end = leafnodedata_.size(); i < end; ++i) {
    int64_t id0 = std::get<0>(leafnodedata_[i]);
    int64_t id1 = std::get<1>(leafnodedata_[i]);
    if (id0 != field0 || id1 != field1) {
      int64_t id = id0 * kOffset_ + id1;
      auto position = static_cast<int64_t>(i);
      auto p3 = std::make_pair(id, position);
      leafdata_map_.insert(p3);
      field0 = id;
      field1 = position;
    }
  }

  // treenode ids, some are roots_, and roots_ have no parents
  std::unordered_map<int64_t, int64_t> parents;  // holds count of all who point to you
  std::unordered_map<int64_t, int64_t> indices;
  // add all the nodes to a map, and the ones that have parents are not roots_
  std::unordered_map<int64_t, int64_t>::iterator it;
  for (size_t i = 0, end = nodes_treeids_.size(); i < end; ++i) {
    // make an index to look up later
    int64_t id = nodes_treeids_[i] * kOffset_ + nodes_nodeids_[i];
    auto position = static_cast<int64_t>(i);
    auto p3 = std::make_pair(id, position);
    indices.insert(p3);
    it = parents.find(id);
    if (it == parents.end()) {
      // start counter at 0
      auto b = (int64_t)0L;
      auto p1 = std::make_pair(id, b);
      parents.insert(p1);
    }
  }
  // all true nodes arent roots_
  for (size_t i = 0, end = nodes_truenodeids_.size(); i < end; ++i) {
    if (nodes_modes_[i] == NODE_MODE::LEAF) continue;
    // they must be in the same tree
    int64_t id = nodes_treeids_[i] * kOffset_ + nodes_truenodeids_[i];
    it = parents.find(id);
    it->second++;
  }
  // all false nodes arent roots_
  for (size_t i = 0, end = nodes_falsenodeids_.size(); i < end; ++i) {
    if (nodes_modes_[i] == NODE_MODE::LEAF) continue;
    // they must be in the same tree
    int64_t id = nodes_treeids_[i] * kOffset_ + nodes_falsenodeids_[i];
    it = parents.find(id);
    it->second++;
  }
  // find all the nodes that dont have other nodes pointing at them
  for (auto& parent : parents) {
    if (parent.second == 0) {
      int64_t id = parent.first;
      it = indices.find(id);
      roots_.push_back(it->second);
    }
  }
  class_count_ = classlabels_int64s_.size();
}


void get_max_weight(const std::map<int64_t, float>& classes, int64_t& maxclass, float& maxweight) {
  maxclass = -1;
  maxweight = 0.f;
  for (auto& classe : classes) {
    if (maxclass == -1 || classe.second > maxweight) {
      maxclass = classe.first;
      maxweight = classe.second;
    }
  }
}


void get_weight_class_positive(std::map<int64_t, float>& classes, float& pos_weight) {
  auto it_classes = classes.find(1);
  pos_weight = it_classes == classes.end()
                   ? (classes.size() > 0 ? classes[0] : 0.f)  // only 1 class
                   : it_classes->second;
}


int64_t _set_score_binary(int64_t i,
                          int& write_additional_scores,
                          bool weights_are_all_positive_,
                          std::map<int64_t, float>& classes,
                          const std::vector<int64_t>& classes_labels_,
                          const std::set<int64_t>& weights_classes_,
                          int64_t positive_label, int64_t negative_label) {
  float pos_weight;
  get_weight_class_positive(classes, pos_weight);
  if (classes_labels_.size() == 2 && weights_classes_.size() == 1) {
    if (weights_are_all_positive_) {
      if (pos_weight > 0.5) {
        write_additional_scores = 0;
        return classes_labels_[1];  // positive label
      } else {
        write_additional_scores = 1;
        return classes_labels_[0];  // negative label
      }
    } else {
      if (pos_weight > 0) {
        write_additional_scores = 2;
        return classes_labels_[1];  // positive label
      } else {
        write_additional_scores = 3;
        return classes_labels_[0];  // negative label
      }
    }
  } else if (pos_weight > 0) {
    return positive_label;  // positive label
  } else {
    return negative_label;  // negative label
  }
}


static inline float ErfInv(float x) {
  float sgn = x < 0 ? -1.0f : 1.0f;
  x = (1 - x) * (1 + x);
  float log = std::log(x);
  float v = 2 / (3.14159f * 0.147f) + 0.5f * log;
  float v2 = 1 / (0.147f) * log;
  float v3 = -v + std::sqrt(v * v - v2);
  x = sgn * std::sqrt(v3);
  return x;
}


float ComputeLogistic(float val) {
  float v = 1 / (1 + std::exp(-std::abs(val)));
  return (val < 0) ? (1 - v) : v;
}

static inline float ComputeProbit(float val) {
  return ml_sqrt2 * ErfInv(2 * val - 1);
}

float sigmoid_probability(float score, float proba, float probb) {
  float val = score * proba + probb;
  return 1 - ComputeLogistic(val);  // ref: https://github.com/arnaudsj/libsvm/blob/eaaefac5ebd32d0e07902e1ae740e038eaaf0826/svm.cpp#L1818
}

void ComputeSoftmax(std::vector<float>& values) {
  std::vector<float> newscores;
  // compute exp with negative number to be numerically stable
  float v_max = -std::numeric_limits<float>::max();
  for (float value : values) {
    if (value > v_max)
      v_max = value;
  }
  float this_sum = 0.f;
  for (float value : values) {
    float val2 = std::exp(value - v_max);
    this_sum += val2;
    newscores.push_back(val2);
  }
  for (int64_t k = 0; k < static_cast<int64_t>(values.size()); k++) {
    values[k] = newscores[k] / this_sum;
  }
}


void ComputeSoftmaxZero(std::vector<float>& values) {
  //this function skips zero values (since exp(0) is non zero)
  std::vector<float> newscores;
  // compute exp with negative number to be numerically stable
  float v_max = -std::numeric_limits<float>::max();
  for (float value : values) {
    if (value > v_max)
      v_max = value;
  }
  float exp_neg_v_max = std::exp(-v_max);
  float this_sum = 0.f;
  for (float value : values) {
    if (value > 0.0000001f || value < -0.0000001f) {
      float val2 = std::exp(value - v_max);
      this_sum += val2;
      newscores.push_back(val2);
    } else {
      newscores.push_back(value * exp_neg_v_max);
    }
  }
  for (int64_t k = 0; k < static_cast<int64_t>(values.size()); k++) {
    values[k] = newscores[k] / this_sum;
  }
}


void write_scores(std::vector<float>& scores, POST_EVAL_TRANSFORM post_transform,
                  float* Z, int add_second_class) {
  if (scores.size() >= 2) {
    switch (post_transform) {
      case POST_EVAL_TRANSFORM::PROBIT:
        for (float& score : scores)
          score = ComputeProbit(score);
        break;
      case POST_EVAL_TRANSFORM::LOGISTIC:
        for (float& score : scores)
          score = ComputeLogistic(score);
        break;
      case POST_EVAL_TRANSFORM::SOFTMAX:
        ComputeSoftmax(scores);
        break;
      case POST_EVAL_TRANSFORM::SOFTMAX_ZERO:
        ComputeSoftmaxZero(scores);
        break;
      default:
      case POST_EVAL_TRANSFORM::NONE:
        break;
    }
  } else if (scores.size() == 1) {  //binary case
    if (post_transform == POST_EVAL_TRANSFORM::PROBIT) {
      scores[0] = ComputeProbit(scores[0]);
    } else {
      switch (add_second_class) {
        case 0:  //0=all positive weights, winning class is positive
          scores.push_back(scores[0]);
          scores[0] = 1.f - scores[0];  //put opposite score in positive slot
          break;
        case 1:  //1 = all positive weights, winning class is negative
          scores.push_back(scores[0]);
          scores[0] = 1.f - scores[0];  //put opposite score in positive slot
          break;
        case 2:  //2 = mixed weights, winning class is positive
          if (post_transform == POST_EVAL_TRANSFORM::LOGISTIC) {
            scores.push_back(ComputeLogistic(scores[0]));  //ml_logit(scores[k]);
            scores[0] = ComputeLogistic(-scores[0]);
          } else {
            scores.push_back(scores[0]);
            scores[0] = -scores[0];
          }
          break;
        case 3:  //3 = mixed weights, winning class is negative
          if (post_transform == POST_EVAL_TRANSFORM::LOGISTIC) {
            scores.push_back(ComputeLogistic(scores[0]));  //ml_logit(scores[k]);
            scores[0] = ComputeLogistic(-scores[0]);
          } else {
            scores.push_back(-scores[0]);
          }
          break;
      }
    }
  }
  memcpy(Z, scores.data(), scores.size() * sizeof(float));
}


py::tuple RuntimeTreeEnsembleClassifier::compute(py::array_t<float> X) const {
    // const Tensor& X = *context->Input<Tensor>(0);
    // const TensorShape& x_shape = X.Shape();
    std::vector<int64_t> x_dims;
    arrayshape2vector(x_dims, X);
    if (x_dims.size() != 2)
        throw std::runtime_error("X must have 2 dimensions.");

    // Does not handle 3D tensors
    int64_t stride = x_dims.size() == 1 ? x_dims[0] : x_dims[1];  
    int64_t N = x_dims.size() == 1 ? 1 : x_dims[0];

    // Tensor* Y = context->Output(0, TensorShape({N}));
    // auto* Z = context->Output(1, TensorShape({N, class_count_}));
    py::array_t<int64_t> Y(x_dims[0]);
    py::array_t<float> Z(x_dims[0] * class_count_);

    auto Y_ = Y.mutable_unchecked<1>();
    auto Z_ = Z.mutable_unchecked<1>();
    const float* x_data = X.data(0);

    // for each class
    std::vector<float> scores;
    scores.reserve(class_count_);
    for (int64_t i = 0; i < N; ++i) {
        scores.clear();
        int64_t current_weight_0 = i * stride;
        std::map<int64_t, float> classes;

        // walk each tree from its root
        for (size_t j = 0, end = roots_.size(); j < end; ++j)
            ProcessTreeNode(classes, roots_[j], x_data, current_weight_0);

        float maxweight = 0.f;
        int64_t maxclass = -1;

        // write top class
        int write_additional_scores = -1;
        if (class_count_ > 2) {
            // add base values
            std::map<int64_t, float>::iterator it_classes;
            for (int64_t k = 0, end = static_cast<int64_t>(base_values_.size()); k < end; ++k) {
                it_classes = classes.find(k);
                if (it_classes == classes.end()) {
                    auto p1 = std::make_pair<int64_t&, const float&>(k, base_values_[k]);
                    classes.insert(p1);
                } 
                else {
                    it_classes->second += base_values_[k];
                }
            }
            get_max_weight(classes, maxclass, maxweight);
            Y_(i) = classlabels_int64s_[maxclass];
        }
        else { // binary case
            if (base_values_.size() == 2) {
                // add base values
                std::map<int64_t, float>::iterator it_classes;
                it_classes = classes.find(1);
                if (it_classes == classes.end()) {
                    // base_value_[0] is not used. It assumes base_value[0] == base_value[1] in this case.
                    // The specification does not forbid it but does not say what the output should be in that case.
                    std::map<int64_t, float>::iterator it_classes0 = classes.find(0);
                    classes[1] = base_values_[1] + it_classes0->second;
                    it_classes0->second = -classes[1];
                }
                else {
                    // binary as multiclass
                    it_classes->second += base_values_[1];
                    classes[0] += base_values_[0];
                }
            }
            Y_(i) = _set_score_binary(i, write_additional_scores,
                              weights_are_all_positive_,
                              classes, classlabels_int64s_,
                              weights_classes_, (int64_t)1, (int64_t)0);            
        }
        // write float values, might not have all the classes in the output yet
        // for example a 10 class case where we only found 2 classes in the leaves
        if (weights_classes_.size() == static_cast<size_t>(class_count_)) {
            for (int64_t k = 0; k < class_count_; ++k) {
                auto it_classes = classes.find(k);
                scores.push_back(it_classes != classes.end() ? it_classes->second : 0.f);
            }
        } 
        else {
            for (auto& classe : classes)
                scores.push_back(classe.second);
        }
        write_scores(scores, post_transform_, (float*)Z_.data(i * class_count_),
                     write_additional_scores);
    }
    return py::make_tuple(Y, Z);
}

void RuntimeTreeEnsembleClassifier::ProcessTreeNode(std::map<int64_t, float>& classes,
                                                    int64_t treeindex,
                                                    const float* x_data,
                                                    int64_t feature_base) const {
  // walk down tree to the leaf
  auto mode = nodes_modes_[treeindex];
  int64_t loopcount = 0;
  int64_t root = treeindex;
  while (mode != NODE_MODE::LEAF) {
    float val = x_data[feature_base + nodes_featureids_[treeindex]];
    bool tracktrue = true;
    tracktrue = missing_tracks_true_.size() != nodes_truenodeids_.size()
                ? false
                : missing_tracks_true_[treeindex] && std::isnan(static_cast<float>(val));
    float threshold = nodes_values_[treeindex];
    switch (mode) {
      case NODE_MODE::BRANCH_LEQ:
        treeindex = val <= threshold || tracktrue ? nodes_truenodeids_[treeindex] : nodes_falsenodeids_[treeindex];
        break;
      case NODE_MODE::BRANCH_LT:
        treeindex = val < threshold || tracktrue ? nodes_truenodeids_[treeindex] : nodes_falsenodeids_[treeindex];
        break;
      case NODE_MODE::BRANCH_GTE:
        treeindex = val >= threshold || tracktrue ? nodes_truenodeids_[treeindex] : nodes_falsenodeids_[treeindex];
        break;
      case NODE_MODE::BRANCH_GT:
        treeindex = val > threshold || tracktrue ? nodes_truenodeids_[treeindex] : nodes_falsenodeids_[treeindex];
        break;
      case NODE_MODE::BRANCH_EQ:
        treeindex = val == threshold || tracktrue ? nodes_truenodeids_[treeindex] : nodes_falsenodeids_[treeindex];
        break;
      case NODE_MODE::BRANCH_NEQ:
        treeindex = val != threshold || tracktrue ? nodes_truenodeids_[treeindex] : nodes_falsenodeids_[treeindex];
        break;
      default: {
        std::ostringstream err_msg;
        err_msg << "Invalid mode of value: " << static_cast<std::underlying_type<NODE_MODE>::type>(mode);
        throw std::runtime_error(err_msg.str());
      }
    }
    treeindex = treeindex + root;
    mode = nodes_modes_[treeindex];
    loopcount++;
    if (loopcount > kMaxTreeDepth_) 
        break;
  }
  // should be at leaf
  int64_t id = nodes_treeids_[treeindex] * kOffset_ + nodes_nodeids_[treeindex];
  auto it_lp = leafdata_map_.find(id);
  if (it_lp == leafdata_map_.end())  // if not found, simply return
    return;

  int64_t index = it_lp->second;
  int64_t treeid = std::get<0>(leafnodedata_[index]);
  int64_t nodeid = std::get<1>(leafnodedata_[index]);
  while (treeid == nodes_treeids_[treeindex] && nodeid == nodes_nodeids_[treeindex]) {
    int64_t classid = std::get<2>(leafnodedata_[index]);
    float weight = std::get<3>(leafnodedata_[index]);
    std::map<int64_t, float>::iterator it_classes;
    it_classes = classes.find(classid);
    if (it_classes != classes.end()) {
      it_classes->second += weight;
    } else {
      auto p1 = std::make_pair(classid, weight);
      classes.insert(p1);
    }
    ++index;
    // some tree node will be last
    if (index >= static_cast<int64_t>(leafnodedata_.size())) {
      break;
    }
    treeid = std::get<0>(leafnodedata_[index]);
    nodeid = std::get<1>(leafnodedata_[index]);
  }
}

#ifndef SKIP_PYTHON

PYBIND11_MODULE(op_tree_ensemble_classifier_, m) {
	m.doc() =
    #if defined(__APPLE__)
    "Implements runtime for operator TreeEnsembleClassifier."
    #else
    R"pbdoc(Implements runtime for operator TreeEnsembleClassifier. The code is inspired from
`tree_ensemble_classifier.cc <https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_classifier.cc>`_
in :epkg:`onnxruntime`.)pbdoc"
    #endif
    ;

    py::class_<RuntimeTreeEnsembleClassifier> cl (m, "RuntimeTreeEnsembleClassifier",
        R"pbdoc(Implements runtime for operator TreeEnsembleClassifier. The code is inspired from
`tree_ensemble_classifier.cc <https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_classifier.cc>`_
in :epkg:`onnxruntime`.)pbdoc");

    cl.def(py::init<>());
    cl.def_readonly("roots_", &RuntimeTreeEnsembleClassifier::roots_,
                    "Returns the roots indices.");
    cl.def("init", &RuntimeTreeEnsembleClassifier::init,
           "Initializes the runtime with the ONNX attributes in alphabetical order.");
    cl.def("compute", &RuntimeTreeEnsembleClassifier::compute,
           "Computes the predictions for the random forest.");
}

#endif