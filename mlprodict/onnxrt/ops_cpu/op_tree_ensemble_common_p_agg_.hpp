#pragma once

// Inspired from 
// https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_regressor.cc.

#if !defined(_CRT_SECURE_NO_WARNINGS)
#define _CRT_SECURE_NO_WARNINGS
#endif

#include <vector>
#include <thread>
#include <iterator>
#include <algorithm>

#ifndef SKIP_PYTHON
//#include <pybind11/iostream.h>
#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/numpy.h>
//#include <numpy/arrayobject.h>

namespace py = pybind11;
#endif

#include "op_common_.hpp"


struct TreeNodeElementId {
    int tree_id;
    int node_id;
    inline bool operator == (const TreeNodeElementId& xyz) const {
        return (tree_id == xyz.tree_id) && (node_id == xyz.node_id);
    }
    inline bool operator < (const TreeNodeElementId& xyz) const {
        return ((tree_id < xyz.tree_id) || (
                tree_id == xyz.tree_id && node_id < xyz.node_id));
    }
};


template<typename NTYPE>
struct SparseValue {
    int64_t i;
    NTYPE value;
};


enum MissingTrack {
    NONE,
    TRUE,
    FALSE
};


template<typename NTYPE>
struct TreeNodeElement {
    TreeNodeElementId id;
    int feature_id;
    NTYPE value;
    NTYPE hitrates;
    NODE_MODE mode;
    TreeNodeElement *truenode;
    TreeNodeElement *falsenode;
    MissingTrack missing_tracks;
    std::vector<SparseValue<NTYPE>> weights;
    
    bool is_not_leave;
    bool is_missing_track_true;
    
    int64_t get_sizeof() {
        return sizeof(TreeNodeElement) + weights.size() * sizeof(SparseValue<NTYPE>);
    }
};


template<typename NTYPE>
class _Aggregator
{
    protected:

        size_t n_trees_;
        int64_t n_targets_or_classes_;
        POST_EVAL_TRANSFORM post_transform_;
        const std::vector<NTYPE> * base_values_;
        NTYPE origin_;
        bool use_base_values_;

    public:

        inline _Aggregator(size_t n_trees,
                           const int64_t& n_targets_or_classes,
                           POST_EVAL_TRANSFORM post_transform,
                           const std::vector<NTYPE> * base_values) : 
                n_trees_(n_trees), n_targets_or_classes_(n_targets_or_classes),
                post_transform_(post_transform), base_values_(base_values) {
            origin_ = base_values_->size() == 1 ? (*base_values_)[0] : 0.f;
            use_base_values_ = base_values_->size() == (size_t)n_targets_or_classes_;
        }

        const char * name() const { return "_Aggregator"; }
        
        // 1 output

        inline void ProcessTreeNodePrediction1(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                               unsigned char* has_predictions) const {}

        inline void MergePrediction1(NTYPE* predictions, unsigned char* has_predictions,
                                     NTYPE* predictions2, unsigned char* has_predictions2) const {}

        inline void FinalizeScores1(NTYPE* Z, NTYPE& val,
                                    unsigned char& has_scores,
                                    int64_t * Y = 0) const {
            val = has_scores ? (val + origin_) : origin_;
            *Z = post_transform_ == POST_EVAL_TRANSFORM::PROBIT ? ComputeProbit(val) : val;
        }

        // N outputs
        
        void ProcessTreeNodePrediction(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                       unsigned char* has_predictions) const {}

        void MergePrediction(int64_t n,
                             NTYPE* predictions, unsigned char* has_predictions,
                             NTYPE* predictions2, unsigned char* has_predictions2) const {}

        void FinalizeScores(std::vector<NTYPE>& scores,
                            std::vector<unsigned char>& has_scores,
                            NTYPE* Z, int add_second_class,
                            int64_t * Y = 0) const {
            NTYPE val;
            for (int64_t jt = 0; jt < n_targets_or_classes_; ++jt) {
                val = use_base_values_ ? (*base_values_)[jt] : 0.f;
                val += has_scores[jt] ? scores[jt] : 0;
                scores[jt] = val;
            }
            write_scores(scores, post_transform_, Z, add_second_class);
        }
};


/////////////
// regression
/////////////


template<typename NTYPE>
class _AggregatorSum : public _Aggregator<NTYPE>
{
    // has_score is not used.
    public:

        inline _AggregatorSum<NTYPE>(size_t n_trees,
                                     const int64_t& n_targets_or_classes,
                                     POST_EVAL_TRANSFORM post_transform,
                                     const std::vector<NTYPE> * base_values) :
            _Aggregator<NTYPE>(n_trees, n_targets_or_classes,
                               post_transform, base_values) { }

        const char * name() const { return "_AggregatorSum"; }

        // 1 output
                               
        inline void ProcessTreeNodePrediction1(NTYPE* predictions,
                                               TreeNodeElement<NTYPE> * root,
                                               unsigned char* has_predictions) const {
            *predictions += root->weights[0].value;
        }

        inline void MergePrediction1(NTYPE* predictions, unsigned char* has_predictions,
                                     const NTYPE* predictions2, const unsigned char* has_predictions2) const {
            *predictions += *predictions2;
        }

        inline void FinalizeScores1(NTYPE* Z, NTYPE& val,
                                    unsigned char& has_scores,
                                    int64_t * Y = 0) const {
            val += this->origin_;
            *Z = this->post_transform_ == POST_EVAL_TRANSFORM::PROBIT ? ComputeProbit(val) : val;
        }

        // N outputs
        
        void ProcessTreeNodePrediction(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                       unsigned char* has_predictions) const {
            for(auto it = root->weights.cbegin(); it != root->weights.cend(); ++it) {
                predictions[it->i] += it->value;
                has_predictions[it->i] = 1;
            }
        }

        void MergePrediction(int64_t n, NTYPE* predictions, unsigned char* has_predictions,
                             const NTYPE* predictions2, const unsigned char* has_predictions2) const {
            for(int64_t i = 0; i < n; ++i) {
                if (has_predictions2[i]) {
                    predictions[i] += predictions2[i];
                    has_predictions[i] = 1;
                }
            }
        }

        void FinalizeScores(std::vector<NTYPE>& scores,
                            std::vector<unsigned char>& has_scores,
                            NTYPE* Z, int add_second_class,
                            int64_t * Y = 0) const {
            if (this->use_base_values_) {
                auto it = scores.begin();
                auto it2 = this->base_values_->cbegin();
                for (; it != scores.end(); ++it, ++it2)
                    *it += *it2;
            }
            write_scores(scores, this->post_transform_, Z, add_second_class);
        }
};


template<typename NTYPE>
class _AggregatorAverage : public _AggregatorSum<NTYPE>
{
    public:
        
        inline _AggregatorAverage<NTYPE>(size_t n_trees,
                                     const int64_t& n_targets_or_classes,
                                     POST_EVAL_TRANSFORM post_transform,
                                     const std::vector<NTYPE> * base_values) :
            _AggregatorSum<NTYPE>(n_trees, n_targets_or_classes,
                                  post_transform, base_values) { }

        const char * name() const { return "_AggregatorAverage"; }

        inline void FinalizeScores1(NTYPE* Z, NTYPE& val,
                                    unsigned char& has_scores,
                                    int64_t * Y = 0) const {
            val /= this->n_trees_;
            val += this->origin_;
            *Z = this->post_transform_ == POST_EVAL_TRANSFORM::PROBIT ? ComputeProbit(val) : val;
        }

        void FinalizeScores(std::vector<NTYPE>& scores,
                            std::vector<unsigned char>& has_scores,
                            NTYPE* Z, int add_second_class,
                            int64_t * Y = 0) const {
            if (this->use_base_values_) {
                auto it = scores.begin();
                auto it2 = this->base_values_->cbegin();
                for (; it != scores.end(); ++it, ++it2)
                    *it = *it / this->n_trees_ + *it2;
            }
            else {                
                for (auto it = scores.begin(); it != scores.end(); ++it)
                    *it /= this->n_trees_;
            }
            write_scores(scores, this->post_transform_, Z, add_second_class);
        }        
};


template<typename NTYPE>
class _AggregatorMin : public _Aggregator<NTYPE>
{
    public:

        inline _AggregatorMin<NTYPE>(size_t n_trees,
                                     const int64_t& n_targets_or_classes,
                                     POST_EVAL_TRANSFORM post_transform,
                                     const std::vector<NTYPE> * base_values) :
            _Aggregator<NTYPE>(n_trees, n_targets_or_classes,
                               post_transform, base_values) { }

        const char * name() const { return "_AggregatorMin"; }

        // 1 output
                               
        inline void ProcessTreeNodePrediction1(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                               unsigned char* has_predictions) const {
            *predictions = (!(*has_predictions) || root->weights[0].value < *predictions) 
                                    ? root->weights[0].value : *predictions;
            *has_predictions = 1;
        }

        inline void MergePrediction1(NTYPE* predictions, unsigned char* has_predictions,
                                       const NTYPE* predictions2, const unsigned char* has_predictions2) const {
            if (*has_predictions2) {
                *predictions = *has_predictions && (*predictions < *predictions2)
                                    ? *predictions
                                    : *predictions2;
                *has_predictions = 1;
            }
        }

        // N outputs
        
        void ProcessTreeNodePrediction(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                       unsigned char* has_predictions) const {
            for(auto it = root->weights.cbegin(); it != root->weights.cend(); ++it) {
                predictions[it->i] = (!has_predictions[it->i] || it->value < predictions[it->i]) 
                                        ? it->value : predictions[it->i];
                has_predictions[it->i] = 1;
            }
        }

        void MergePrediction(int64_t n, NTYPE* predictions, unsigned char* has_predictions,
                             const NTYPE* predictions2, const unsigned char* has_predictions2) const {
            for(int64_t i = 0; i < n; ++i) {
                if (has_predictions2[i]) {
                    predictions[i] = has_predictions[i] && (predictions[i] < predictions2[i])
                                        ? predictions[i]
                                        : predictions2[i];
                    has_predictions[i] = 1;
                }
            }
        }

};


template<typename NTYPE>
class _AggregatorMax : public _Aggregator<NTYPE>
{
    public:

        inline _AggregatorMax<NTYPE>(size_t n_trees,
                                     const int64_t& n_targets_or_classes,
                                     POST_EVAL_TRANSFORM post_transform,
                                     const std::vector<NTYPE> * base_values) :
            _Aggregator<NTYPE>(n_trees, n_targets_or_classes,
                               post_transform, base_values) { }

        const char * name() const { return "_AggregatorMax"; }

        // 1 output

        inline void ProcessTreeNodePrediction1(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                               unsigned char* has_predictions) const {
            *predictions = (!(*has_predictions) || root->weights[0].value > *predictions) 
                                    ? root->weights[0].value : *predictions;
            *has_predictions = 1;
        }

        inline void MergePrediction1(NTYPE* predictions, unsigned char* has_predictions,
                                     const NTYPE* predictions2, const unsigned char* has_predictions2) const {
            if (*has_predictions2) {
                *predictions = *has_predictions && (*predictions > *predictions2)
                                    ? *predictions
                                    : *predictions2;
                *has_predictions = 1;
            }
        }

        // N outputs

        void ProcessTreeNodePrediction(NTYPE* predictions, TreeNodeElement<NTYPE> * root,
                                       unsigned char* has_predictions) const {
            for(auto it = root->weights.cbegin(); it != root->weights.cend(); ++it) {
                predictions[it->i] = (!has_predictions[it->i] || it->value > predictions[it->i]) 
                                        ? it->value : predictions[it->i];
                has_predictions[it->i] = 1;
            }
        }

        void MergePrediction(int64_t n, NTYPE* predictions, unsigned char* has_predictions,
                             NTYPE* predictions2, unsigned char* has_predictions2) const {
            for(int64_t i = 0; i < n; ++i) {
                if (has_predictions2[i]) {
                    predictions[i] = has_predictions[i] && (predictions[i] > predictions2[i])
                                        ? predictions[i]
                                        : predictions2[i];
                    has_predictions[i] = 1;
                }
            }
        }
};


/////////////////
// classification
/////////////////


template<typename NTYPE>
class _AggregatorClassifier : public _AggregatorSum<NTYPE>
{
    private:

        const std::vector<int64_t> * class_labels_;
        bool binary_case_;
        bool weights_are_all_positive_;
        int64_t positive_label_;
        int64_t negative_label_;

    public:

        inline _AggregatorClassifier(size_t n_trees,
                                     const int64_t& n_targets_or_classes,
                                     POST_EVAL_TRANSFORM post_transform,
                                     const std::vector<NTYPE> * base_values,
                                     const std::vector<int64_t> * class_labels,
                                     bool binary_case,
                                     bool weights_are_all_positive,
                                     int64_t positive_label = 1,
                                     int64_t negative_label = 0) :
            _AggregatorSum<NTYPE>(n_trees, n_targets_or_classes,
                                  post_transform, base_values),
            class_labels_(class_labels), binary_case_(binary_case),
            weights_are_all_positive_(weights_are_all_positive),
            positive_label_(positive_label), negative_label_(negative_label) { }
            
        const char * name() const { return "_AggregatorClassifier"; }

        void get_max_weight(const std::vector<NTYPE>& classes, 
                            const std::vector<unsigned char>& has_scores, 
                            int64_t& maxclass, NTYPE& maxweight) const {
            maxclass = -1;
            maxweight = (NTYPE)0;
            typename std::vector<NTYPE>::const_iterator it;
            typename std::vector<unsigned char>::const_iterator itb;
            for (it = classes.cbegin(), itb = has_scores.cbegin();
               it != classes.cend(); ++it, ++itb) {
                if (*itb && (maxclass == -1 || *it > maxweight)) {
                    maxclass = (int64_t)(it - classes.cbegin());
                    maxweight = *it;
                }
            }
        }

        inline int64_t _set_score_binary(int& write_additional_scores,
                                         const NTYPE* classes,
                                         const unsigned char* has_scores) const {
            NTYPE pos_weight = has_scores[1]
                                ? classes[1]
                                : (has_scores[0] ? classes[0] : (NTYPE)0);  // only 1 class
            if (binary_case_) {
                if (weights_are_all_positive_) {
                    if (pos_weight > 0.5) {
                        write_additional_scores = 0;
                        return (*class_labels_)[1];  // positive label
                    } 
                    else {
                        write_additional_scores = 1;
                        return (*class_labels_)[0];  // negative label
                    }
                }
                else {
                    if (pos_weight > 0) {
                        write_additional_scores = 2;
                        return (*class_labels_)[1];  // positive label
                    }
                    else {
                        write_additional_scores = 3;
                        return (*class_labels_)[0];  // negative label
                    }
                }
            }
            return (pos_weight > 0) 
                        ? positive_label_   // positive label
                        : negative_label_;  // negative label
        }

        // 1 output
        
        inline void FinalizeScores1(NTYPE* Z, NTYPE& val,
                                    unsigned char& has_score,
                                    int64_t * Y = 0) const {
            NTYPE scores[2];
            unsigned char has_scores[2] = {1, 0};

            int write_additional_scores = -1;
            if (this->base_values_->size() == 2) {
                // add base values
                scores[1] = (*(this->base_values_))[1] + val;
                scores[0] = -scores[1];
                //has_score = true;
                has_scores[1] = 1;
            }
            else if (this->base_values_->size() == 1) {
                // ONNX is vague about two classes and only one base_values.
                scores[0] = val + (*(this->base_values_))[0];
                //if (!has_scores[1])
                //scores.pop_back();
                scores[0] = val;
            }
            else if (this->base_values_->size() == 0) {
                //if (!has_score)
                //  scores.pop_back();
                scores[0] = val;
            }

            *Y = _set_score_binary(write_additional_scores, &(scores[0]), has_scores);
            write_scores2(scores, this->post_transform_, Z,
                          write_additional_scores);            
        }

        // N outputs
        
        void FinalizeScores(std::vector<NTYPE>& scores,
                            std::vector<unsigned char>& has_scores,
                            NTYPE* Z, int add_second_class,
                            int64_t * Y = 0) const {
            NTYPE maxweight = (NTYPE)0;
            int64_t maxclass = -1;

            int write_additional_scores = -1;
            if (this->n_targets_or_classes_ > 2) {
                // add base values
                for (int64_t k = 0, end = static_cast<int64_t>(this->base_values_->size()); k < end; ++k) {
                    if (!has_scores[k]) {
                      has_scores[k] = true;
                      scores[k] = (*(this->base_values_))[k];
                    }
                    else {
                        scores[k] += (*(this->base_values_))[k];
                    }
                }
                get_max_weight(scores, has_scores, maxclass, maxweight);
                *Y = (*class_labels_)[maxclass];
            }
            else { // binary case
                if (this->base_values_->size() == 2) {
                    // add base values
                    if (has_scores[1]) {
                        // base_value_[0] is not used.
                        // It assumes base_value[0] == base_value[1] in this case.
                        // The specification does not forbid it but does not
                        // say what the output should be in that case.
                        scores[1] = (*(this->base_values_))[1] + scores[0];
                        scores[0] = -scores[1];
                        has_scores[1] = true;
                    }
                    else {
                        // binary as multiclass
                        scores[1] += (*(this->base_values_))[1];
                        scores[0] += (*(this->base_values_))[0];
                    }
                }
                else if (this->base_values_->size() == 1) {
                    // ONNX is vague about two classes and only one base_values.
                    scores[0] += (*(this->base_values_))[0];
                    if (!has_scores[1])
                      scores.pop_back();
                }
                else if (this->base_values_->size() == 0) {
                    if (!has_scores[1])
                      scores.pop_back();
                }

                *Y = _set_score_binary(write_additional_scores, &(scores[0]), &(has_scores[0]));
            }

            write_scores(scores, this->post_transform_, Z, write_additional_scores);
        }
};
