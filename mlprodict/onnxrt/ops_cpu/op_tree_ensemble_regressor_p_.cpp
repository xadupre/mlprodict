// Inspired from 
// https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_regressor.cc.

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

#if USE_OPENMP
#include <omp.h>
#endif

namespace py = pybind11;
#endif

#include "op_common_.hpp"

template<typename NTYPE>
class RuntimeTreeEnsembleRegressorP
{
    public:
        
        struct TreeNodeElementId {
            int tree_id;
            int node_id;
            bool operator == (const TreeNodeElementId& xyz) const {
                return (tree_id == xyz.tree_id) && (node_id == xyz.node_id);
            }
            bool operator < (const TreeNodeElementId& xyz) const {
                return ((tree_id < xyz.tree_id) || (
                        tree_id == xyz.tree_id && node_id < xyz.node_id));
            }
        };

        struct SparseValue {
            int64_t i;
            NTYPE value;
        };
        
        enum MissingTrack {
            NONE,
            TRUE,
            FALSE
        };

        struct TreeNodeElement {
            TreeNodeElementId id;
            int feature_id;
            NTYPE value;
            NTYPE hitrates;
            NODE_MODE mode;
            TreeNodeElement *truenode;
            TreeNodeElement *falsenode;
            MissingTrack missing_tracks;

            std::vector<SparseValue> weights;
        };
        
        // tree_ensemble_regressor.h
        std::vector<NTYPE> base_values_;
        int64_t n_targets_;
        POST_EVAL_TRANSFORM post_transform_;
        AGGREGATE_FUNCTION aggregate_function_;
        int64_t nbnodes_;
        TreeNodeElement* nodes_;
        std::vector<TreeNodeElement*> roots_;

        int64_t max_tree_depth_;
        int64_t nbtrees_;
        bool same_mode_;
        bool has_missing_tracks_;
        const int64_t kOffset_ = 4000000000L;
    
    public:
        
        RuntimeTreeEnsembleRegressorP();
        ~RuntimeTreeEnsembleRegressorP();

        void init(
            const std::string &aggregate_function,
            py::array_t<NTYPE> base_values,
            int64_t n_targets,
            py::array_t<int64_t> nodes_falsenodeids,
            py::array_t<int64_t> nodes_featureids,
            py::array_t<NTYPE> nodes_hitrates,
            py::array_t<int64_t> nodes_missing_value_tracks_true,
            const std::vector<std::string>& nodes_modes,
            py::array_t<int64_t> nodes_nodeids,
            py::array_t<int64_t> nodes_treeids,
            py::array_t<int64_t> nodes_truenodeids,
            py::array_t<NTYPE> nodes_values,
            const std::string& post_transform,
            py::array_t<int64_t> target_ids,
            py::array_t<int64_t> target_nodeids,
            py::array_t<int64_t> target_treeids,
            py::array_t<NTYPE> target_weights);
        
        py::array_t<NTYPE> compute(py::array_t<NTYPE> X) const;

        void ProcessTreeNode(NTYPE* predictions, TreeNodeElement * root,
                             const NTYPE* x_data,
                             unsigned char* has_predictions) const;
    
        std::string runtime_options();
        std::vector<std::string> get_nodes_modes() const;
        
        int omp_get_max_threads();
        
        py::array_t<int> debug_threshold(py::array_t<NTYPE> values) const;

        py::array_t<NTYPE> compute_tree_outputs(py::array_t<NTYPE> values) const;

    private:

        void compute_gil_free(const std::vector<int64_t>& x_dims, int64_t N, int64_t stride,
                              const py::array_t<NTYPE>& X, py::array_t<NTYPE>& Z) const;
};


template<typename NTYPE>
RuntimeTreeEnsembleRegressorP<NTYPE>::RuntimeTreeEnsembleRegressorP() {
}


template<typename NTYPE>
RuntimeTreeEnsembleRegressorP<NTYPE>::~RuntimeTreeEnsembleRegressorP() {
    delete [] nodes_;
}


template<typename NTYPE>
std::string RuntimeTreeEnsembleRegressorP<NTYPE>::runtime_options() {
    std::string res;
#ifdef USE_OPENMP
    res += "OPENMP";
#endif
    return res;
}


template<typename NTYPE>
int RuntimeTreeEnsembleRegressorP<NTYPE>::omp_get_max_threads() {
#if USE_OPENMP
    return ::omp_get_max_threads();
#else
    return 1;
#endif
}


template<typename NTYPE>
void RuntimeTreeEnsembleRegressorP<NTYPE>::init(
            const std::string &aggregate_function,
            py::array_t<NTYPE> base_values,
            int64_t n_targets,
            py::array_t<int64_t> nodes_falsenodeids,
            py::array_t<int64_t> nodes_featureids,
            py::array_t<NTYPE> nodes_hitrates,
            py::array_t<int64_t> nodes_missing_value_tracks_true,
            const std::vector<std::string>& nodes_modes,
            py::array_t<int64_t> nodes_nodeids,
            py::array_t<int64_t> nodes_treeids,
            py::array_t<int64_t> nodes_truenodeids,
            py::array_t<NTYPE> nodes_values,
            const std::string& post_transform,
            py::array_t<int64_t> target_ids,
            py::array_t<int64_t> target_nodeids,
            py::array_t<int64_t> target_treeids,
            py::array_t<NTYPE> target_weights) {

    aggregate_function_ = to_AGGREGATE_FUNCTION(aggregate_function);        
    array2vector(base_values_, base_values, NTYPE);
    n_targets_ = n_targets;

    std::vector<int64_t> nodes_treeids_;
    std::vector<int64_t> nodes_nodeids_;
    std::vector<int64_t> nodes_featureids_;
    std::vector<NTYPE> nodes_values_;
    std::vector<NTYPE> nodes_hitrates_;
    std::vector<NODE_MODE> nodes_modes_;
    std::vector<int64_t> nodes_truenodeids_;
    std::vector<int64_t> nodes_falsenodeids_;
    std::vector<int64_t> missing_tracks_true_;

    std::vector<int64_t> target_nodeids_;
    std::vector<int64_t> target_treeids_;
    std::vector<int64_t> target_ids_;
    std::vector<NTYPE> target_weights_;    
    
    array2vector(nodes_falsenodeids_, nodes_falsenodeids, int64_t);
    array2vector(nodes_featureids_, nodes_featureids, int64_t);
    array2vector(nodes_hitrates_, nodes_hitrates, NTYPE);
    array2vector(missing_tracks_true_, nodes_missing_value_tracks_true, int64_t);
    array2vector(nodes_truenodeids_, nodes_truenodeids, int64_t);
    //nodes_modes_names_ = nodes_modes;
    array2vector(nodes_nodeids_, nodes_nodeids, int64_t);
    array2vector(nodes_treeids_, nodes_treeids, int64_t);
    array2vector(nodes_truenodeids_, nodes_truenodeids, int64_t);
    array2vector(nodes_values_, nodes_values, NTYPE);
    array2vector(nodes_truenodeids_, nodes_truenodeids, int64_t);
    post_transform_ = to_POST_EVAL_TRANSFORM(post_transform);
    array2vector(target_ids_, target_ids, int64_t);
    array2vector(target_nodeids_, target_nodeids, int64_t);
    array2vector(target_treeids_, target_treeids, int64_t);
    array2vector(target_weights_, target_weights, NTYPE);
    
    // additional members
    nodes_modes_.resize(nodes_modes.size());
    same_mode_ = true;
    size_t fpos = -1;
    for(size_t i = 0; i < nodes_modes.size(); ++i) {
        nodes_modes_[i] = to_NODE_MODE(nodes_modes[i]);
        if (nodes_modes_[i] == NODE_MODE::LEAF)
            continue;
        if (fpos == -1) {
            fpos = i;
            continue;
        }
        if (nodes_modes_[i] != nodes_modes_[fpos])
            same_mode_ = false;
    }

    max_tree_depth_ = 1000;
    
    // filling nodes

    /*
    std::vector<TreeNodeElement<NTYPE>> nodes_;
    std::vector<TreeNodeElement<NTYPE>*> roots_;
    */
    nbnodes_ = nodes_treeids_.size();
    nodes_ = new TreeNodeElement[(int)nbnodes_];
    roots_.clear();
    std::map<TreeNodeElementId, TreeNodeElement*> idi;
    size_t i;
    
    for (i = 0; i < nodes_treeids_.size(); ++i) {
        TreeNodeElement * node = nodes_ + i;
        node->id.tree_id = (int)nodes_treeids_[i];
        node->id.node_id = (int)nodes_nodeids_[i];
        node->feature_id = (int)nodes_featureids_[i];
        node->value = nodes_values_[i];
        node->hitrates = i < nodes_hitrates_.size() ? nodes_hitrates_[i] : -1;
        node->mode = nodes_modes_[i];
        node->truenode = NULL; // nodes_truenodeids_[i];
        node->falsenode = NULL; // nodes_falsenodeids_[i];
        node->missing_tracks = i < (size_t)missing_tracks_true_.size()
                                    ? (missing_tracks_true_[i] == 1 
                                            ? MissingTrack::TRUE : MissingTrack::FALSE)
                                    : MissingTrack::NONE;
        if (idi.find(node->id) != idi.end()) {
            char buffer[1000];
            sprintf(buffer, "Node %d in tree %d is already there.", (int)node->id.node_id, (int)node->id.tree_id);
            throw std::runtime_error(buffer);
        }
        idi.insert(std::pair<TreeNodeElementId, TreeNodeElement*>(node->id, node));
    }

    TreeNodeElementId coor;
    TreeNodeElement * it;
    for(i = 0; i < (size_t)nbnodes_; ++i) {
        it = nodes_ + i;
        if (it->mode == NODE_MODE::LEAF)
            continue;
        coor.tree_id = it->id.tree_id;
        coor.node_id = (int)nodes_truenodeids_[i];

        auto found = idi.find(coor);
        if (found == idi.end()) {
            char buffer[1000];
            sprintf(buffer, "Unable to find node %d-%d (truenode).", (int)coor.tree_id, (int)coor.node_id);
            throw std::runtime_error(buffer);
        }
        if (coor.node_id >= 0 && coor.node_id < nbnodes_) {
            it->truenode = found->second;
            if ((it->truenode->id.tree_id != it->id.tree_id) ||
                (it->truenode->id.node_id == it->id.node_id)) {
                char buffer[1000];
                sprintf(buffer, "truenode [%d] is pointing either to itself [node id=%d], either to another tree [%d!=%d-%d].",
                    (int)i, (int)it->id.node_id, (int)it->id.tree_id,
                    (int)it->truenode->id.tree_id, (int)it->truenode->id.tree_id);
                throw std::runtime_error(buffer);
            }
        }
        else it->truenode = NULL;

        coor.node_id = (int)nodes_falsenodeids_[i];
        found = idi.find(coor);
        if (found == idi.end()) {
            char buffer[1000];
            sprintf(buffer, "Unable to find node %d-%d (falsenode).", (int)coor.tree_id, (int)coor.node_id);
            throw std::runtime_error(buffer);
        }
        if (coor.node_id >= 0 && coor.node_id < nbnodes_) {
            it->falsenode = found->second;
            if ((it->falsenode->id.tree_id != it->id.tree_id) ||
                (it->falsenode->id.node_id == it->id.node_id )) {
                throw std::runtime_error("One falsenode is pointing either to itself, either to another tree.");
                char buffer[1000];
                sprintf(buffer, "falsenode [%d] is pointing either to itself [node id=%d], either to another tree [%d!=%d-%d].",
                    (int)i, (int)it->id.node_id, (int)it->id.tree_id,
                    (int)it->falsenode->id.tree_id, (int)it->falsenode->id.tree_id);
                throw std::runtime_error(buffer);
            }
        }
        else it->falsenode = NULL;
    }
    
    int64_t previous = -1;
    for(i = 0; i < (size_t)nbnodes_; ++i) {
        if ((previous == -1) || (previous != nodes_[i].id.tree_id))
            roots_.push_back(nodes_ + i);
        previous = nodes_[i].id.tree_id;
    }
        
    TreeNodeElementId ind;
    SparseValue w;
    for (i = 0; i < target_nodeids_.size(); i++) {
        ind.tree_id = (int)target_treeids_[i];
        ind.node_id = (int)target_nodeids_[i];
        if (idi.find(ind) == idi.end()) {
            char buffer[1000];
            sprintf(buffer, "Unable to find node %d-%d (weights).", (int)coor.tree_id, (int)coor.node_id);
            throw std::runtime_error(buffer);
        }
        w.i = target_ids_[i];
        w.value = target_weights_[i];
        idi[ind]->weights.push_back(w);
    }
    
    nbtrees_ = roots_.size();
    has_missing_tracks_ = missing_tracks_true_.size() == nodes_truenodeids_.size();
}


template<typename NTYPE>
std::vector<std::string> RuntimeTreeEnsembleRegressorP<NTYPE>::get_nodes_modes() const {
    std::vector<std::string> res;
    for(int i = 0; i < (int)nbnodes_; ++i)
        res.push_back(to_str(nodes_[i].mode));
    return res;
}


template<typename NTYPE>
py::array_t<NTYPE> RuntimeTreeEnsembleRegressorP<NTYPE>::compute(py::array_t<NTYPE> X) const {
    // const Tensor& X = *context->Input<Tensor>(0);
    // const TensorShape& x_shape = X.Shape();    
    std::vector<int64_t> x_dims;
    arrayshape2vector(x_dims, X);
    if (x_dims.size() != 2)
        throw std::runtime_error("X must have 2 dimensions.");

    // Does not handle 3D tensors
    bool xdims1 = x_dims.size() == 1;
    int64_t stride = xdims1 ? x_dims[0] : x_dims[1];  
    int64_t N = xdims1 ? 1 : x_dims[0];

    // Tensor* Y = context->Output(0, TensorShape({N}));
    // auto* Z = context->Output(1, TensorShape({N, class_count_}));
    py::array_t<NTYPE> Z(x_dims[0] * n_targets_);

    {
        py::gil_scoped_release release;
        compute_gil_free(x_dims, N, stride, X, Z);
    }
    return Z;
}


py::detail::unchecked_mutable_reference<float, 1> _mutable_unchecked1(py::array_t<float>& Z) {
    return Z.mutable_unchecked<1>();
}


py::detail::unchecked_mutable_reference<double, 1> _mutable_unchecked1(py::array_t<double>& Z) {
    return Z.mutable_unchecked<1>();
}


template<typename NTYPE>
void RuntimeTreeEnsembleRegressorP<NTYPE>::compute_gil_free(
                const std::vector<int64_t>& x_dims, int64_t N, int64_t stride,
                const py::array_t<NTYPE>& X, py::array_t<NTYPE>& Z) const {

    // expected primary-expression before ')' token
    auto Z_ = _mutable_unchecked1(Z); // Z.mutable_unchecked<(size_t)1>();
                    
    const NTYPE* x_data = X.data(0);

    if (n_targets_ == 1) {
        NTYPE origin = base_values_.size() == 1 ? base_values_[0] : 0.f;
        if (N == 1) {
            NTYPE scores = 0;
            unsigned char has_scores = 0;

            #ifdef USE_OPENMP
            #pragma omp parallel for
            #endif
            for (int64_t j = 0; j < nbtrees_; ++j)
                ProcessTreeNode(&scores, roots_[j], x_data, &has_scores);

            NTYPE val = has_scores
                    ? (aggregate_function_ == AGGREGATE_FUNCTION::AVERAGE
                        ? scores / roots_.size()
                        : scores) + origin
                    : origin;
            *((NTYPE*)Z_.data(0)) = (post_transform_ == POST_EVAL_TRANSFORM::PROBIT) 
                                        ? ComputeProbit(val) : val;
        }
        else {
            NTYPE scores;
            unsigned char has_scores;
            NTYPE val;
            #ifdef USE_OPENMP
            #pragma omp parallel for private(scores, has_scores, val)
            #endif
            for (int64_t i = 0; i < N; ++i) {
                scores = 0;
                has_scores = 0;
  
                for (size_t j = 0; j < (size_t)nbtrees_; ++j)
                    ProcessTreeNode(&scores, roots_[j], x_data + i * stride, &has_scores);
  
                val = has_scores
                      ? (aggregate_function_ == AGGREGATE_FUNCTION::AVERAGE
                          ? scores / roots_.size()
                          : scores) + origin
                      : origin;
                *((NTYPE*)Z_.data(i)) = (post_transform_ == POST_EVAL_TRANSFORM::PROBIT) 
                            ? ComputeProbit(val) : val;
            }
        }
    }
    else {
        if (N == 1) {
            std::vector<NTYPE> scores(n_targets_, (NTYPE)0);
            std::vector<unsigned char> has_scores(n_targets_, 0);

            #ifdef USE_OPENMP
            #pragma omp parallel for
            #endif
            for (int64_t j = 0; j < nbtrees_; ++j)
                ProcessTreeNode(scores.data(), roots_[j], x_data, has_scores.data());

            std::vector<NTYPE> outputs(n_targets_);
            NTYPE val;
            for (int64_t j = 0; j < n_targets_; ++j) {
                //reweight scores based on number of voters
                val = base_values_.size() == (size_t)n_targets_ ? base_values_[j] : 0.f;
                val = (has_scores[j]) 
                        ?  val + (aggregate_function_ == AGGREGATE_FUNCTION::AVERAGE
                                    ? scores[j] / roots_.size()
                                    : scores[j])
                        : val;
                outputs[j] = val;
            }
            write_scores(outputs, post_transform_, (NTYPE*)Z_.data(0), -1);
        }
        else {
            std::vector<NTYPE> scores(n_targets_, (NTYPE)0);
            std::vector<NTYPE> outputs(n_targets_);
            std::vector<unsigned char> has_scores(n_targets_, 0);
            int64_t current_weight_0;
            NTYPE val;

            #ifdef USE_OPENMP
            #pragma omp parallel for firstprivate(scores, has_scores, outputs) private(val, current_weight_0)
            #endif
            for (int64_t i = 0; i < N; ++i) {
                current_weight_0 = i * stride;
                std::fill(scores.begin(), scores.end(), (NTYPE)0);
                std::fill(outputs.begin(), outputs.end(), (NTYPE)0);
                std::fill(has_scores.begin(), has_scores.end(), 0);

                for (size_t j = 0; j < roots_.size(); ++j)
                    ProcessTreeNode(scores.data(), roots_[j], x_data + current_weight_0,
                                    has_scores.data());

                for (int64_t j = 0; j < n_targets_; ++j) {
                    val = base_values_.size() == (size_t)n_targets_ ? base_values_[j] : 0.f;
                    val = (has_scores[j]) 
                            ?  val + (aggregate_function_ == AGGREGATE_FUNCTION::AVERAGE
                                        ? scores[j] / roots_.size()
                                        : scores[j])
                            : val;
                    outputs[j] = val;
                }
                write_scores(outputs, post_transform_, (NTYPE*)Z_.data(i * n_targets_), -1);
            }
        }
    }
}


#define TREE_FIND_VALUE(CMP) \
    if (has_missing_tracks_) { \
        while (root->mode != NODE_MODE::LEAF && loopcount >= 0) { \
            val = x_data[root->feature_id]; \
            root = (val CMP root->value || \
                    (root->missing_tracks == MissingTrack::TRUE && \
                        std::isnan(static_cast<NTYPE>(val)) )) \
                        ? root->truenode : root->falsenode; \
            --loopcount; \
        } \
    } \
    else { \
        while (root->mode != NODE_MODE::LEAF && loopcount >= 0) { \
            val = x_data[root->feature_id]; \
            root = val CMP root->value ? root->truenode : root->falsenode; \
            --loopcount; \
        } \
    }


template<typename NTYPE>
void RuntimeTreeEnsembleRegressorP<NTYPE>::ProcessTreeNode(
        NTYPE* predictions, TreeNodeElement * root,
        const NTYPE* x_data,
        unsigned char* has_predictions) const {
    bool tracktrue;
    NTYPE val;
    if (same_mode_) {
        int64_t loopcount = max_tree_depth_;
        switch(root->mode) {
            case NODE_MODE::BRANCH_LEQ:
                TREE_FIND_VALUE(<=)
                break;
            case NODE_MODE::BRANCH_LT:
                TREE_FIND_VALUE(<)
                break;
            case NODE_MODE::BRANCH_GTE:
                TREE_FIND_VALUE(>=)
                break;
            case NODE_MODE::BRANCH_GT:
                TREE_FIND_VALUE(>)
                break;
            case NODE_MODE::BRANCH_EQ:
                TREE_FIND_VALUE(==)
                break;
            case NODE_MODE::BRANCH_NEQ:
                TREE_FIND_VALUE(!=)
                break;
            case NODE_MODE::LEAF:
                break;
            default: {
                std::ostringstream err_msg;
                err_msg << "Invalid mode of value: " << static_cast<std::underlying_type<NODE_MODE>::type>(root->mode);
                throw std::runtime_error(err_msg.str());
            }
        }
    }
    else {  // Different rules to compare to node thresholds.
        int64_t loopcount = 0;
        NTYPE threshold;
        while ((root->mode != NODE_MODE::LEAF) && (loopcount <= max_tree_depth_)) {
            val = x_data[root->feature_id];
            tracktrue = root->missing_tracks == MissingTrack::TRUE &&
                        std::isnan(static_cast<NTYPE>(val));
            threshold = root->value;
            switch (root->mode) {
                case NODE_MODE::BRANCH_LEQ:
                    root = val <= threshold || tracktrue
                              ? root->truenode
                              : root->falsenode;
                    break;
                case NODE_MODE::BRANCH_LT:
                    root = val < threshold || tracktrue
                              ? root->truenode
                              : root->falsenode;
                    break;
                case NODE_MODE::BRANCH_GTE:
                    root = val >= threshold || tracktrue
                              ? root->truenode
                              : root->falsenode;
                    break;
                case NODE_MODE::BRANCH_GT:
                    root = val > threshold || tracktrue
                              ? root->truenode
                              : root->falsenode;
                    break;
                case NODE_MODE::BRANCH_EQ:
                    root = val == threshold || tracktrue
                              ? root->truenode
                              : root->falsenode;
                    break;
                case NODE_MODE::BRANCH_NEQ:
                    root = val != threshold || tracktrue
                              ? root->truenode
                              : root->falsenode;
                    break;
                default: {
                    std::ostringstream err_msg;
                    err_msg << "Invalid mode of value: " << static_cast<std::underlying_type<NODE_MODE>::type>(root->mode);
                    throw std::runtime_error(err_msg.str());
                }
            }
            ++loopcount;
        }      
    }
  
    //should be at leaf
    switch(aggregate_function_) {
        case AGGREGATE_FUNCTION::AVERAGE:
        case AGGREGATE_FUNCTION::SUM:
            for(auto it = root->weights.begin(); it != root->weights.end(); ++it) {
                predictions[it->i] += it->value;
                has_predictions[it->i] = 1;
            }
            break;
        case AGGREGATE_FUNCTION::MIN:
            for(auto it = root->weights.begin(); it != root->weights.end(); ++it) {
                predictions[it->i] = (!has_predictions[it->i] || it->value < predictions[it->i]) 
                                        ? it->value : predictions[it->i];
                has_predictions[it->i] = 1;
            }
            break;
        case AGGREGATE_FUNCTION::MAX:
            for(auto it = root->weights.begin(); it != root->weights.end(); ++it) {
                predictions[it->i] = (!has_predictions[it->i] || it->value > predictions[it->i]) 
                                        ? it->value : predictions[it->i];
                has_predictions[it->i] = 1;
            }
            break;
    }
}


template<typename NTYPE>
py::array_t<int> RuntimeTreeEnsembleRegressorP<NTYPE>::debug_threshold(
        py::array_t<NTYPE> values) const {
    std::vector<int> result(values.size() * nbnodes_);
    const NTYPE* x_data = values.data(0);
    const NTYPE* end = x_data + values.size();
    const NTYPE* pv;
    auto itb = result.begin();
    auto nodes_end = nodes_ + nbnodes_;
    for(auto it = nodes_; it != nodes_end; ++it)
        for(pv=x_data; pv != end; ++pv, ++itb)
            *itb = *pv <= it->value ? 1 : 0;
    std::vector<ssize_t> shape = { nbnodes_, values.size() };
    std::vector<ssize_t> strides = { (ssize_t)(values.size()*sizeof(int)),
                                     (ssize_t)sizeof(int) };
    return py::array_t<NTYPE>(
        py::buffer_info(
            &result[0],
            sizeof(NTYPE),
            py::format_descriptor<NTYPE>::format(),
            2,
            shape,                                   /* shape of the matrix       */
            strides                                  /* strides for each axis     */
        ));
}


template<typename NTYPE>
py::array_t<NTYPE> RuntimeTreeEnsembleRegressorP<NTYPE>::compute_tree_outputs(py::array_t<NTYPE> X) const {
    
    std::vector<int64_t> x_dims;
    arrayshape2vector(x_dims, X);
    if (x_dims.size() != 2)
        throw std::runtime_error("X must have 2 dimensions.");

    int64_t stride = x_dims.size() == 1 ? x_dims[0] : x_dims[1];  
    int64_t N = x_dims.size() == 1 ? 1 : x_dims[0];
    
    std::vector<NTYPE> result(N * roots_.size());
    const NTYPE* x_data = X.data(0);
    auto itb = result.begin();

    for (int64_t i=0; i < N; ++i)  //for each class
    {
        int64_t current_weight_0 = i * stride;
        for (size_t j = 0; j < roots_.size(); ++j, ++itb) {
            std::vector<NTYPE> scores(n_targets_, (NTYPE)0);
            std::vector<unsigned char> has_scores(n_targets_, 0);
            ProcessTreeNode(scores.data(), roots_[j], x_data + current_weight_0,
                            has_scores.data());
            *itb = scores[0];
        }
    }
    
    std::vector<ssize_t> shape = { (ssize_t)N, (ssize_t)roots_.size() };
    std::vector<ssize_t> strides = { (ssize_t)(roots_.size()*sizeof(NTYPE)),
                                     (ssize_t)sizeof(NTYPE) };
    return py::array_t<NTYPE>(
        py::buffer_info(
            &result[0],
            sizeof(NTYPE),
            py::format_descriptor<NTYPE>::format(),
            2,
            shape,                                   /* shape of the matrix       */
            strides                                  /* strides for each axis     */
        ));
}


class RuntimeTreeEnsembleRegressorPFloat : public RuntimeTreeEnsembleRegressorP<float> {
    public:
        RuntimeTreeEnsembleRegressorPFloat() : RuntimeTreeEnsembleRegressorP<float>() {}
};


class RuntimeTreeEnsembleRegressorPDouble : public RuntimeTreeEnsembleRegressorP<double> {
    public:
        RuntimeTreeEnsembleRegressorPDouble() : RuntimeTreeEnsembleRegressorP<double>() {}
};


#ifndef SKIP_PYTHON

PYBIND11_MODULE(op_tree_ensemble_regressor_p_, m) {
	m.doc() =
    #if defined(__APPLE__)
    "Implements runtime for operator TreeEnsembleRegressor."
    #else
    R"pbdoc(Implements runtime for operator TreeEnsembleRegressor. The code is inspired from
`tree_ensemble_regressor.cc <https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_Regressor.cc>`_
in :epkg:`onnxruntime`.)pbdoc"
    #endif
    ;

    py::class_<RuntimeTreeEnsembleRegressorPFloat> clf (m, "RuntimeTreeEnsembleRegressorPFloat",
        R"pbdoc(Implements float runtime for operator TreeEnsembleRegressor. The code is inspired from
`tree_ensemble_regressor.cc <https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_Regressor.cc>`_
in :epkg:`onnxruntime`. Supports float only.)pbdoc");

    clf.def(py::init<>());
    clf.def_readonly("roots_", &RuntimeTreeEnsembleRegressorPFloat::roots_,
                     "Returns the roots indices.");
    clf.def("init", &RuntimeTreeEnsembleRegressorPFloat::init,
            "Initializes the runtime with the ONNX attributes in alphabetical order.");
    clf.def("compute", &RuntimeTreeEnsembleRegressorPFloat::compute,
            "Computes the predictions for the random forest.");
    clf.def("runtime_options", &RuntimeTreeEnsembleRegressorPFloat::runtime_options,
            "Returns indications about how the runtime was compiled.");
    clf.def("omp_get_max_threads", &RuntimeTreeEnsembleRegressorPFloat::omp_get_max_threads,
            "Returns omp_get_max_threads from openmp library.");

    clf.def_readonly("base_values_", &RuntimeTreeEnsembleRegressorPFloat::base_values_, "See :ref:`lpyort-TreeEnsembleRegressor`.");
    clf.def_readonly("n_targets_", &RuntimeTreeEnsembleRegressorPFloat::n_targets_, "See :ref:`lpyort-TreeEnsembleRegressor`.");
    clf.def_readonly("post_transform_", &RuntimeTreeEnsembleRegressorPFloat::post_transform_, "See :ref:`lpyort-TreeEnsembleRegressor`.");

    clf.def("debug_threshold", &RuntimeTreeEnsembleRegressorPFloat::debug_threshold,
        "Checks every features against every features against every threshold. Returns a matrix of boolean.");
    clf.def("compute_tree_outputs", &RuntimeTreeEnsembleRegressorPFloat::compute_tree_outputs,
        "Computes every tree output.");
    clf.def_readonly("same_mode_", &RuntimeTreeEnsembleRegressorPFloat::same_mode_,
        "Tells if all nodes applies the same rule for thresholds.");
    clf.def_property_readonly("nodes_modes_", &RuntimeTreeEnsembleRegressorPFloat::get_nodes_modes,
        "Returns the mode for every node.");

    py::class_<RuntimeTreeEnsembleRegressorPDouble> cld (m, "RuntimeTreeEnsembleRegressorPDouble",
        R"pbdoc(Implements double runtime for operator TreeEnsembleRegressor. The code is inspired from
`tree_ensemble_regressor.cc <https://github.com/microsoft/onnxruntime/blob/master/onnxruntime/core/providers/cpu/ml/tree_ensemble_Regressor.cc>`_
in :epkg:`onnxruntime`. Supports double only.)pbdoc");

    cld.def(py::init<>());
    cld.def_readonly("roots_", &RuntimeTreeEnsembleRegressorPDouble::roots_,
                     "Returns the roots indices.");
    cld.def("init", &RuntimeTreeEnsembleRegressorPDouble::init,
            "Initializes the runtime with the ONNX attributes in alphabetical order.");
    cld.def("compute", &RuntimeTreeEnsembleRegressorPDouble::compute,
            "Computes the predictions for the random forest.");
    cld.def("runtime_options", &RuntimeTreeEnsembleRegressorPDouble::runtime_options,
            "Returns indications about how the runtime was compiled.");
    cld.def("omp_get_max_threads", &RuntimeTreeEnsembleRegressorPDouble::omp_get_max_threads,
            "Returns omp_get_max_threads from openmp library.");

    cld.def_readonly("base_values_", &RuntimeTreeEnsembleRegressorPDouble::base_values_, "See :ref:`lpyort-TreeEnsembleRegressorDouble`.");
    cld.def_readonly("n_targets_", &RuntimeTreeEnsembleRegressorPDouble::n_targets_, "See :ref:`lpyort-TreeEnsembleRegressorDouble`.");
    cld.def_readonly("post_transform_", &RuntimeTreeEnsembleRegressorPDouble::post_transform_, "See :ref:`lpyort-TreeEnsembleRegressorDouble`.");
    // cld.def_readonly("leafnode_data_", &RuntimeTreeEnsembleRegressorPDouble::leafnode_data_, "See :ref:`lpyort-TreeEnsembleRegressorDouble`.");
    
    cld.def("debug_threshold", &RuntimeTreeEnsembleRegressorPDouble::debug_threshold,
        "Checks every features against every features against every threshold. Returns a matrix of boolean.");
    cld.def("compute_tree_outputs", &RuntimeTreeEnsembleRegressorPDouble::compute_tree_outputs,
        "Computes every tree output.");
    cld.def_readonly("same_mode_", &RuntimeTreeEnsembleRegressorPDouble::same_mode_,
        "Tells if all nodes applies the same rule for thresholds.");
    cld.def_property_readonly("nodes_modes_", &RuntimeTreeEnsembleRegressorPDouble::get_nodes_modes,
        "Returns the mode for every node.");
}

#endif
