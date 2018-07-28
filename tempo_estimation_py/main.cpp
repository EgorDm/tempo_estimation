#include <pybind11/pybind11.h>
#include "tempogram_wrapper.h"

namespace py = pybind11;

PYBIND11_MODULE(tempo_estimation_py, m) {
    m.def("novelty_curve_to_tempogram_dft", &tempogram_wrapper::novelty_curve_to_tempogram_dft);
    m.def("normalize_feature", &tempogram_wrapper::normalize_feature);
};